#include "BashConsoleSession.h"

#include <array>
#include <cerrno>
#include <cstring>

#if defined(_WIN32)

BashConsoleSession::BashConsoleSession() = default;
BashConsoleSession::~BashConsoleSession() = default;

bool BashConsoleSession::start(const std::filesystem::path&) {
    return false;
}

bool BashConsoleSession::running() {
    return false;
}

bool BashConsoleSession::writeInput(const std::string&) {
    return false;
}

std::string BashConsoleSession::readAvailable(int) {
    return "Interactive console PTY is not implemented on Windows yet.\n";
}

void BashConsoleSession::stop() {
}

#else

#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

BashConsoleSession::BashConsoleSession() = default;

BashConsoleSession::~BashConsoleSession() {
    stop();
}

bool BashConsoleSession::start(const std::filesystem::path& cwd) {
    std::lock_guard lock(mutex_);
    if (masterFd_ >= 0) {
        return true;
    }

    int master = -1;
    winsize size{};
    size.ws_row = 32;
    size.ws_col = 120;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &size);
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        chdir(cwd.string().c_str());
        setenv("TERM", "xterm-256color", 1);
        setenv("NO_COLOR", "1", 1);
        setenv("CLICOLOR", "0", 1);
        execlp("bash", "bash", "-i", nullptr);
        _exit(127);
    }

    fcntl(master, F_SETFL, fcntl(master, F_GETFL, 0) | O_NONBLOCK);
    masterFd_ = master;
    childPid_ = static_cast<int>(pid);
    return true;
}

bool BashConsoleSession::running() {
    std::lock_guard lock(mutex_);
    if (childPid_ <= 0) {
        return false;
    }

    int status = 0;
    const pid_t result = waitpid(childPid_, &status, WNOHANG);
    if (result == 0) {
        return true;
    }
    if (masterFd_ >= 0) {
        close(masterFd_);
    }
    masterFd_ = -1;
    childPid_ = -1;
    return false;
}

bool BashConsoleSession::writeInput(const std::string& input) {
    std::lock_guard lock(mutex_);
    if (masterFd_ < 0) {
        return false;
    }
    return write(masterFd_, input.data(), input.size()) >= 0;
}

std::string BashConsoleSession::readAvailable(int timeoutMs) {
    int fd = -1;
    {
        std::lock_guard lock(mutex_);
        fd = masterFd_;
    }
    if (fd < 0) {
        return {};
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    std::string output;
    const int ready = select(fd + 1, &readSet, nullptr, nullptr, &timeout);
    if (ready <= 0) {
        return output;
    }

    std::array<char, 4096> buffer{};
    while (true) {
        const ssize_t count = read(fd, buffer.data(), buffer.size());
        if (count > 0) {
            output.append(buffer.data(), static_cast<std::size_t>(count));
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
    return output;
}

void BashConsoleSession::stop() {
    std::lock_guard lock(mutex_);
    if (childPid_ > 0) {
        kill(childPid_, SIGHUP);
        waitpid(childPid_, nullptr, 0);
    }
    if (masterFd_ >= 0) {
        close(masterFd_);
    }
    masterFd_ = -1;
    childPid_ = -1;
}

#endif
