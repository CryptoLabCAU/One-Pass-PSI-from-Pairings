#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>  // for open
#include <unistd.h> // for close
#include <mutex>
#include <vector>
#include <algorithm>
#include <random>

#include <chrono>
#include "utils.hpp"

using namespace std;
using namespace chrono;

int main(int argc, char *argv[])
{
    int port = 0, n = 0, m = 0;
    int N = 0, M = 0;

    if (parse_command_line(argc, argv, port, n, m) != EXIT_SUCCESS)
        port = 1234;

    N = (1 << n);
    M = (1 << m);

    std::cout << "Semi-Honest Model - Receiver:" << std::endl;
    std::cout << "N = " << N << ", M = " << M << std::endl;

    // Init for TCP
    int client_socket;
    struct sockaddr_in server_address;
    socklen_t addr_size;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    // Configure settings of the server address
    // Address family is Internet
    server_address.sin_family = AF_INET;

    // Set port number, using htons function
    server_address.sin_port = htons(port);

    // Set IP address to localhost
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));
    addr_size = sizeof(server_address);

    // Connect the socket to the server using the address
    if (connect(client_socket, reinterpret_cast<struct sockaddr *>(&server_address), addr_size) < 0)
    {
        std::cerr << "TCP/IP: Connection failed" << std::endl;
        return -1;
    }
    else
        std::cout << "TCP/IP: Connection established successfully" << std::endl;

    // CRS Generation Phase
    if (core_init() != RLC_OK || pc_param_set_any() != RLC_OK)
    {
        printf("Relic initialization failed.\n");
        return 1;
    }

    g1_t g1;
    g1_null(g1);
    g1_new(g1);
    g1_get_gen(g1);

    g2_t g2;
    g2_null(g2);
    g2_new(g2);
    g2_get_gen(g2);

    bn_t p;
    bn_null(p);
    bn_new(p);
    pc_get_ord(p);

    bn_t s;
    bn_null(s);
    bn_new(s);
    bn_read_str(s, S, strlen(S), 16);

    // Gamma = g2^s
    g2_t Gamma;
    g2_null(Gamma);
    g2_new(Gamma);
    g2_mul(Gamma, g2, s);

    // Data Setup Phase
    std::vector<bn_t> y(M);

    for (int i = 0; i < M; ++i)
    {
        bn_null(y[i]);
        bn_new(y[i]);

        bn_rand_mod(y[i], p);
    }

    // Receiver-set Registration Phase
    bn_t rho;
    bn_null(rho);
    bn_new(rho);
    bn_rand_mod(rho, p);

    std::vector<g1_t> K(M);

    // K_i = F(y_i)^rho in G1
    for (int i = 0; i < M; i++)
    {
        int dlen = Hash_F(y[i], K[i]);
        g1_mul(K[i], K[i], rho);
    }

    // K_i = F(y_i)^(rho * s)
    for (int i = 0; i < M; i++)
        g1_mul(K[i], K[i], s);

    // K_i = F(y_i)^(s)
    bn_t inv_rho;
    bn_null(inv_rho);
    bn_new(inv_rho);
    bn_mod_inv(inv_rho, rho, p);

    for (int i = 0; i < M; i++)
        g1_mul(K[i], K[i], inv_rho);


    send(client_socket, "Start", strlen("Start"), 0);

    // Communication Phase

    std::vector<uint8_t *> R(N);

    uint8_t PSI[4 * RLC_PC_BYTES + 1];
    memset(PSI, 0x00, 4 * RLC_PC_BYTES + 1);

    if (recv(client_socket, PSI, 4 * RLC_PC_BYTES + 1, 0) < 0)
        printf("Receive failed\n");

    for (int i = 0; i < N; i++)
    {
        R[i] = new uint8_t[SHA256_DIGEST_LENGTH];
        if (recv(client_socket, R[i], SHA256_DIGEST_LENGTH, 0) < 0)
            printf("Receive failed\n");
    }

    // Intersection Extraction Phase

    g2_t psi;
    g2_null(psi);
    g2_new(psi);
    g2_read_bin(psi, PSI, 4 * RLC_PC_BYTES + 1);

    int intersection = 0;
    // std::vector<bn_t> Z;

    std::sort(R.begin(), R.end(), compareArrays);
    for (int i = 0; i < M; i++)
    {
        gt_t e;
        gt_null(e);
        gt_new(e);

        pc_map(e, K[i], psi);

        uint8_t *temp;
        int dlen = Hash_H(e, temp);

        // Find intersection
        if (binarySearch(R, temp))
            intersection++;

        free(temp);
        gt_free(e);
    }


    for (int i = 0; i < M; ++i)
    {
        g2_free(K[i]);
        bn_free(y[i]);
    }

    for (int i = 0; i < N; i++)
        delete[] R[i];

    g1_free(g1);
    g2_free(g2);
    bn_free(p);
    bn_free(s);
    g1_free(Gamma);
    bn_free(rho);
    bn_free(inv_rho);
    g1_free(psi);

    core_clean();

    return 0;
}