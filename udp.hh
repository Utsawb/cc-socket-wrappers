#ifndef UDP_HH
#define UDP_HH

#include <algorithm>
#include <arpa/inet.h>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace jj
{
    inline auto assert_throw(bool condition, const std::string &msg) -> void
    {
        if (condition == false)
        {
            throw std::runtime_error(msg);
        }
    }

    inline auto operator<<(std::vector<char> &vec, const std::string str) -> std::vector<char> &
    {
        std::copy(str.begin(), str.end() + 1, vec.begin());
        return vec;
    }

    /*  A header only wrapper around the C-Style UDP Socket API */
    class UDP
    {
        public:
            enum Side
            {
                CLIENT,
                SERVER
            };

        private:
            int sock_fd = 0;
            struct sockaddr_in sock_conf = {0};
            socklen_t sock_conf_len = sizeof(struct sockaddr_in);
            Side side;

        public:
            /*  Create a new UDP object, if side == 0 then client, and side == 1 then server */
            UDP(const std::string ip_addr, const std::string port, const Side &side) : side(side)
            {
                sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
                assert_throw(this->sock_fd != -1, "Failed to create socket");

                sock_conf.sin_family = AF_INET;
                sock_conf.sin_port = htons(std::stoul(port));
                sock_conf_len = sizeof(sock_conf);

                if (side == Side::SERVER)
                {
                    sock_conf.sin_addr.s_addr = INADDR_ANY;
                    int ret = bind(sock_fd, (struct sockaddr *)&sock_conf, sock_conf_len);
                    assert_throw(ret != -1, "Failed to bind to port");
                }
                else if (side == Side::CLIENT)
                {
                    sock_conf.sin_addr.s_addr = inet_addr(ip_addr.c_str());
                }
            }

            /* Closes the socket */
            ~UDP()
            {
                close(sock_fd);
            }

            /* UDP should not be copied, since this is undefined behavior */
            UDP(const UDP &obj) = delete;

            /* UDP move constructor */
            UDP(UDP &&obj)
            {
                sock_fd = obj.sock_fd;
                sock_conf = obj.sock_conf;
                sock_conf_len = obj.sock_conf_len;
                side = obj.side;

                obj.sock_fd = -1;
                std::fill((std::byte *)&sock_conf, (std::byte *)&sock_conf + sizeof(sock_conf), std::byte{0});
                obj.sock_conf_len = -1;
            }

            /* UDP move assignment */
            auto operator=(UDP &&obj) -> UDP &
            {
                if (this == &obj)
                {
                    return *this;
                }

                sock_fd = obj.sock_fd;
                sock_conf = obj.sock_conf;
                sock_conf_len = obj.sock_conf_len;
                side = obj.side;

                obj.sock_fd = -1;
                std::fill((std::byte *)&sock_conf, (std::byte *)&sock_conf + sizeof(sock_conf), std::byte{0});
                obj.sock_conf_len = -1;

                return *this;
            }

            /* UDP should not be copied, since this is undefined behavior */
            auto operator=(const UDP &obj) -> UDP & = delete;

            /* Takes a vector obj and sends it through the socket */
            template <typename T> friend auto operator<<(UDP &udp, const std::vector<T> &obj) -> UDP &
            {
                int nbytes = sendto(udp.sock_fd, obj.data(), obj.size() * sizeof(T), 0,
                                    (struct sockaddr *)&udp.sock_conf, udp.sock_conf_len);
                assert_throw(nbytes != -1, "Failed to write to socket");
                return udp;
            }

            /*  Takes a vector obj and writes to it, uses capacity as the buffer limit and resizes
                the vector to the number of bytes received from the socket */
            template <typename T> friend auto operator>>(UDP &udp, std::vector<T> &obj) -> UDP &
            {
                obj.resize(obj.capacity());
                int nbytes = recv(udp.sock_fd, obj.data(), obj.capacity() * sizeof(T), 0);
                assert_throw(nbytes != -1, "Failed to read from socket");
                obj.resize(nbytes / sizeof(T));
                return udp;
            }

            /*  Takes a string obj and writes it to the socket */
            friend auto operator<<(UDP &udp, const std::string &obj) -> UDP &
            {
                int nbytes = sendto(udp.sock_fd, obj.c_str(), obj.size() + 1, 0, (struct sockaddr *)&udp.sock_conf,
                                    udp.sock_conf_len);
                assert_throw(nbytes != -1, "Failed to write to socket");
                return udp;
            }

            /*  Takes a string obj and writes to it, uses capacity as the buffer limit and resizes
                the string to the number of bytes received from the socket.
                Since resize trucates the string, ensure that it is resized upon reuse. */
            friend auto operator>>(UDP &udp, std::string &obj) -> UDP &
            {
                int nbytes = recv(udp.sock_fd, obj.data(), obj.capacity(), 0);
                obj.resize(nbytes);
                assert_throw(nbytes != -1, "Failed to read from socket");
                return udp;
            }

            /*  A generic write that takes any object writes it though the socket.
                Ensure that the object is trivial since this only writes using the address
                and size of the object */
            template <typename T> friend auto operator<<(UDP &udp, const T &obj) -> UDP &
            {
                int nbytes =
                    sendto(udp.sock_fd, &obj, sizeof(T), 0, (struct sockaddr *)&udp.sock_conf, udp.sock_conf_len);

                assert_throw(nbytes != -1, "Failed to write to socket");
                return udp;
            }

            /*  A generic read that takes any object reads it from the socket.
                Ensure that the object is trivial since this only writes using the address
                and size of the object */
            template <typename T> friend auto operator>>(UDP &udp, T &obj) -> UDP &
            {
                int nbytes = recv(udp.sock_fd, &obj, sizeof(obj), 0);
                assert_throw(nbytes != -1, "Failed to read from socket");
                return udp;
            }

            /* A direct wrapper around the underlying send function */
            auto write(const void *msg, const std::size_t size) -> ssize_t
            {
                int nbytes = sendto(sock_fd, msg, size, 0, (struct sockaddr *)&sock_conf, sock_conf_len);

                assert_throw(nbytes != -1, "Failed to write to socket");
                return nbytes;
            }

            /* A direct wrapper around the underlying write function */
            auto read(void *msg, std::size_t size) -> ssize_t
            {
                int nbytes = recv(sock_fd, msg, size, 0);
                assert_throw(nbytes != -1, "Failed to read from socket");
                return nbytes;
            }
    };
} // namespace jj

#endif
