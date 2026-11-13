#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

constexpr char kRequestFifo[] = "request.fifo";
constexpr char kReplyFifo[] = "reply.fifo";

bool make_fifo(const char* path) {
    if (mkfifo(path, 0600) == 0) return true;
    if (errno != EEXIST) return false;

    struct stat info {};
    return stat(path, &info) == 0 && S_ISFIFO(info.st_mode);
}

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

int main() {
    if (!make_fifo(kRequestFifo) || !make_fifo(kReplyFifo)) {
        std::cerr << "Could not create FIFOs: " << std::strerror(errno) << '\n';
        return 1;
    }

    std::cout << "Echo server ready. Press Ctrl+C to stop.\n";
    while (true) {
        // Opening a FIFO for reading waits for a client to open it for writing.
        int request = open(kRequestFifo, O_RDONLY);
        if (request < 0) {
            if (errno == EINTR) continue;
            std::perror("open request");
            return 1;
        }

        std::string message;
        char buffer[4096];
        while (true) {
            ssize_t n = read(request, buffer, sizeof(buffer));
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) {
                std::perror("read request");
                close(request);
                return 1;
            }
            if (n == 0) break;  // Client closed the request FIFO.
            message.append(buffer, static_cast<size_t>(n));
        }
        close(request);

        int reply = open(kReplyFifo, O_WRONLY);
        if (reply < 0) {
            std::perror("open reply");
            return 1;
        }
        bool ok = write_all(reply, message);
        close(reply);
        if (!ok) {
            std::perror("write reply");
            return 1;
        }
        std::cout << "Echoed: " << message << '\n';
    }
}
