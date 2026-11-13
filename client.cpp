#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

constexpr char kRequestFifo[] = "request.fifo";
constexpr char kReplyFifo[] = "reply.fifo";

bool write_all(int fd, const std::string& message) {
    const char* data = message.data();
    size_t remaining = message.size();
    while (remaining > 0) {
        ssize_t n = write(fd, data, remaining);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        data += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " \"message\"\n";
        return 1;
    }

    int request = open(kRequestFifo, O_WRONLY);
    if (request < 0) {
        std::perror("open request (is the server running?)");
        return 1;
    }
    bool ok = write_all(request, argv[1]);
    close(request);  // EOF tells the server the message is complete.
    if (!ok) {
        std::perror("write request");
        return 1;
    }

    int reply = open(kReplyFifo, O_RDONLY);
    if (reply < 0) {
        std::perror("open reply");
        return 1;
    }
    std::string echoed;
    char buffer[4096];
    while (true) {
        ssize_t n = read(reply, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            std::perror("read reply");
            close(reply);
            return 1;
        }
        if (n == 0) break;
        echoed.append(buffer, static_cast<size_t>(n));
    }
    close(reply);
    std::cout << "Server replied: " << echoed << '\n';
}
