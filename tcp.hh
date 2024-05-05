#ifndef TCP_HH
#define TCP_HH

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
    /* Helper to use assert like syntax to throw an error */
    inline auto assert_throw(bool condition, const std::string &msg) -> void
    {
        if (condition == false)
        {
            throw std::runtime_error(msg);
        }
    }

    /* Helper to load a string into a vector */
    inline auto operator<<(std::vector<char> &vec, const std::string str) -> std::vector<char> &
    {
        std::copy(str.begin(), str.end() + 1, vec.begin());
        return vec;
    }

    /*  A header only wrapper around the C-Style TCP Socket API. */
    class TCP
    {
        public:
            enum Side
            {
                CLIENT,
                SERVER,
                CONNECTION
            };

        private:
            int sock_fd;
            struct sockaddr_in sock_conf;
            socklen_t sock_conf_len;
            Side side;

            /* Used internally to create a new TCP instance for an accepted connection */
            TCP(int sock_fd) : sock_fd(sock_fd), sock_conf_len(-1), side(Side::CONNECTION)
            {
                std::fill((std::byte *)&sock_conf, (std::byte *)&sock_conf + sizeof(sock_conf), std::byte{0});
            }

        public:
            /*  Create a new TCP object, if ip_addr is empty then a server will create, otherwise a client will
                be created. */
            TCP(const std::string ip_addr, const std::string port, const Side &side) : side(side)
            {
                sock_fd = socket(AF_INET, SOCK_STREAM, 0);
                assert_throw(this->sock_fd != -1, "Failed to create socket");

                sock_conf.sin_family = AF_INET;
                sock_conf.sin_port = htons(std::stoul(port));
                sock_conf_len = sizeof(sock_conf);

                if (side == Side::SERVER)
                {
                    sock_conf.sin_addr.s_addr = inet_addr("0.0.0.0");
                    int ret = bind(sock_fd, (struct sockaddr *)&sock_conf, sock_conf_len);
                    assert_throw(ret != -1, "Failed to bind to port");
                }
                else if (side == Side::CLIENT)
                {
                    sock_conf.sin_addr.s_addr = inet_addr(ip_addr.c_str());
                    int ret = connect(sock_fd, (struct sockaddr *)&sock_conf, sock_conf_len);
                    assert_throw(ret != -1, "Failed to connect to server");
                    sock_conf_len = -1;
                }
            }

            /* Closes the socket */
            ~TCP()
            {
                close(sock_fd);
            }

            /* TCP should not be copied, since this is undefined behavior */
            TCP(const TCP &obj) = delete;

            /* TCP should not be copied, since this is undefined behavior */
            auto operator=(const TCP &obj) -> TCP & = delete;

            /* TCP move constructor */
            TCP(TCP &&obj)
            {
                sock_fd = obj.sock_fd;
                sock_conf = obj.sock_conf;
                sock_conf_len = obj.sock_conf_len;
                side = obj.side;

                obj.sock_fd = -1;
                std::fill((std::byte *)&sock_conf, (std::byte *)&sock_conf + sizeof(sock_conf), std::byte{0});
                obj.sock_conf_len = -1;
            }

            /* TCP move assignment */
            auto operator=(TCP &&obj) -> TCP &
            {
                if (this == &obj)
                {
                    return *this;
                }

                sock_fd = obj.sock_fd;
                sock_conf = obj.sock_conf;
                sock_conf_len = obj.sock_conf_len;

                obj.sock_fd = -1;
                std::fill((std::byte *)&sock_conf, (std::byte *)&sock_conf + sizeof(sock_conf), std::byte{0});
                obj.sock_conf_len = -1;

                return *this;
            }

            /*  Prepare to accept connections on the server, queue_size connections will be queued before
                connections are dropped */
            auto accept_connection(const std::size_t &queue_size) -> TCP
            {
                assert_throw(side == Side::SERVER, "Must accept connection from server");
                int ret = listen(sock_fd, queue_size);
                assert_throw(ret != -1, "Failed to start listener");

                int new_sock = accept(sock_fd, (struct sockaddr *)&sock_conf, &sock_conf_len);
                assert_throw(new_sock != -1, "Failed to accept connection");

                return TCP(new_sock);
            }

            /* Takes a vector obj and sends it through the socket */
            template <typename T> friend auto operator<<(TCP &tcp, const std::vector<T> &obj) -> TCP &
            {
                assert_throw(tcp.side != jj::TCP::Side::SERVER, "Can not write to server socket");
                int nbytes = send(tcp.sock_fd, obj.data(), obj.size() * sizeof(T), 0);
                assert_throw(nbytes != -1, "Failed to write to socket");
                return tcp;
            }

            /*  Takes a vector obj and writes to it, uses capacity as the buffer limit and resizes
                the vector to the number of bytes received from the socket */
            template <typename T> friend auto operator>>(TCP &tcp, std::vector<T> &obj) -> TCP &
            {
                assert_throw(tcp.side != jj::TCP::Side::SERVER, "Can not read from server socket");
                obj.resize(obj.capacity());
                int nbytes = recv(tcp.sock_fd, obj.data(), obj.capacity() * sizeof(T), 0);
                assert_throw(nbytes != -1, "Failed to read from socket");
                obj.resize(nbytes / sizeof(T));
                return tcp;
            }

            /*  Takes a string obj and writes it to the socket */
            friend auto operator<<(TCP &tcp, const std::string &obj) -> TCP &
            {
                assert_throw(tcp.side != jj::TCP::Side::SERVER, "Can not write to server socket");
                int nbytes = send(tcp.sock_fd, obj.c_str(), obj.size() + 1, 0);
                assert_throw(nbytes != -1, "Failed to write to socket");
                return tcp;
            }

            /*  Takes a string obj and writes to it, uses capacity as the buffer limit and resizes
                the string to the number of bytes received from the socket.
                Since resize trucates the string, ensure that it is resized upon reuse. */
            friend auto operator>>(TCP &tcp, std::string &obj) -> TCP &
            {
                assert_throw(tcp.side != jj::TCP::Side::SERVER, "Can not read from server socket");
                int nbytes = recv(tcp.sock_fd, obj.data(), obj.capacity(), 0);
                obj.resize(nbytes);
                assert_throw(nbytes != -1, "Failed to read from socket");
                return tcp;
            }

            /*  A generic write that takes any object writes it though the socket.
                Ensure that the object is trivial since this only writes using the address
                and size of the object */
            template <typename T> friend auto operator<<(TCP &tcp, const T &obj) -> TCP &
            {
                assert_throw(tcp.side != jj::TCP::Side::SERVER, "Can not write to server socket");
                int nbytes = send(tcp.sock_fd, &obj, sizeof(obj), 0);
                assert_throw(nbytes != -1, "Failed to write to socket");
                return tcp;
            }

            /*  A generic read that takes any object reads it from the socket.
                Ensure that the object is trivial since this only writes using the address
                and size of the object */
            template <typename T> friend auto operator>>(TCP &tcp, T &obj) -> TCP &
            {
                assert_throw(tcp.side != jj::TCP::Side::SERVER, "Can not read from server socket");
                int nbytes = recv(tcp.sock_fd, &obj, sizeof(obj), 0);
                assert_throw(nbytes != -1, "Failed to read from socket");
                return tcp;
            }

            /* A direct wrapper around the underlying send function */
            auto write(const void *msg, const std::size_t size) -> ssize_t
            {
                assert_throw(side != Side::SERVER, "Can not write to server socket");
                int nbytes = send(sock_fd, msg, size, 0);
                assert_throw(nbytes != -1, "Failed to write to socket");
                return nbytes;
            }

            /* A direct wrapper around the underlying write function */
            auto read(void *msg, std::size_t size) -> ssize_t
            {
                assert_throw(side != Side::SERVER, "Can not read from server socket");
                int nbytes = recv(sock_fd, msg, size, 0);
                assert_throw(nbytes != -1, "Failed to read from socket");
                return nbytes;
            }
    };
} // namespace jj

#endif
