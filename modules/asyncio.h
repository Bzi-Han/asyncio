#ifndef ASYNCIO_H // !ASYNCIO_H
#define ASYNCIO_H

#include "coroutine/coroutine.h"

#include <WinSock2.h>
#include <Windows.h>

namespace asyncio
{
    using namespace coroutine;

    struct EndPoint
    {
        size_t id;
        SOCKET socket;
        std::string ip;
        uint16_t port;
    };

    task<> print(const std::string_view &content)
    {
        std::cout << content << std::endl;

        co_return;
    }

    task<std::string> lastErrorMessage(int code = 0)
    {
        if (0 == code)
            code = ::WSAGetLastError();

        std::string result(1024, 0);
        auto size = ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)result.data(), (DWORD)result.size(), nullptr);
        result.resize(size);

        co_return result;
    };

    task<bool> wsaStartup(uint8_t majorVersion, uint8_t minorVersion)
    {
        WSADATA wsaData{};

        auto errorCode = ::WSAStartup(MAKEWORD(majorVersion, minorVersion), &wsaData);
        if (!errorCode)
            co_return true;

        co_await print(co_await lastErrorMessage(errorCode));
    }

    task<int> wsaCleanup()
    {
        co_return ::WSACleanup();
    }

    task<SOCKET> socket(int af, int type, int protocol)
    {
        co_return ::socket(af, type, protocol);
    }

    task<int> closesocket(SOCKET socket)
    {
        co_return ::closesocket(socket);
    }

    task<bool> bind(SOCKET socket, const sockaddr_in *name)
    {
        auto errorCode = ::bind(socket, reinterpret_cast<const sockaddr *>(name), sizeof(sockaddr));
        if (!errorCode)
            co_return true;

        co_await print(co_await lastErrorMessage());
    }

    task<bool> bind(SOCKET socket, const std::string_view &ip, uint16_t port)
    {
        sockaddr_in serverConfig{};
        serverConfig.sin_family = PF_INET;
        serverConfig.sin_addr.S_un.S_addr = inet_addr(ip.data());
        serverConfig.sin_port = htons(port);

        co_return co_await bind(socket, &serverConfig);
    }

    task<bool> listen(SOCKET socket, int maxConnect)
    {
        auto errorCode = ::listen(socket, maxConnect);
        if (!errorCode)
            co_return true;

        co_await print(co_await lastErrorMessage());
    }

    task<EndPoint> accept(SOCKET socket)
    {
        sockaddr_in endpointInfo{};
        int sizeOfStruct = sizeof(sockaddr);
        EndPoint result{};
        static size_t id = 0;

        result.socket = ::accept(socket, reinterpret_cast<sockaddr *>(&endpointInfo), &sizeOfStruct);
        if (INVALID_SOCKET == result.socket)
            co_return {};

        result.id = id++;
        result.ip = inet_ntoa(endpointInfo.sin_addr);
        result.port = ntohs(endpointInfo.sin_port);

        co_return result;
    }

    task<bool> connect(SOCKET socket, const std::string_view &ip, uint16_t port)
    {
        sockaddr_in targetServer{};
        targetServer.sin_family = PF_INET;
        targetServer.sin_addr.S_un.S_addr = ::inet_addr(ip.data());
        targetServer.sin_port = ::htons(port);

        auto errorCode = ::connect(socket, reinterpret_cast<sockaddr *>(&targetServer), sizeof(sockaddr_in));
        if (!errorCode)
            co_return true;

        co_await print(co_await lastErrorMessage());
    }

    task<int> select(fd_set *readSockets, fd_set *writeSockets = nullptr, fd_set *exceptSockets = nullptr, const timeval &timeout = {}, int nfds = 0)
    {
        co_return ::select(nfds, readSockets, writeSockets, exceptSockets, &timeout);
    }

    task<bool> send(SOCKET socket, const std::string_view &data)
    {
        WSABUF wsaBuf{};
        WSAOVERLAPPED wsaOverlapped{};

        wsaBuf.buf = const_cast<char *>(data.data());
        wsaBuf.len = static_cast<ULONG>(data.size());
        wsaOverlapped.hEvent = ::WSACreateEvent();
        if (nullptr == wsaOverlapped.hEvent)
        {
            co_await print("send failed: create event error");
            co_return false;
        }

        auto errorCode = ::WSASend(socket, &wsaBuf, 1, nullptr, 0, &wsaOverlapped, nullptr);
        if ((SOCKET_ERROR == errorCode) && (WSA_IO_PENDING != (errorCode = ::WSAGetLastError())))
        {
            co_await print(co_await lastErrorMessage(errorCode));
            co_return false;
        }

        while (WSA_WAIT_EVENT_0 != ::WSAWaitForMultipleEvents(1, &wsaOverlapped.hEvent, true, 0, false))
            co_switch;

        ::WSACloseEvent(wsaOverlapped.hEvent);

        co_return true;
    }

    task<size_t> recv(SOCKET socket, char *buffer, size_t bufferSize)
    {
        WSABUF wsaBuf{};
        DWORD flags = 0, recvBytes = 0;
        WSAOVERLAPPED wsaOverlapped{};

        wsaBuf.buf = buffer;
        wsaBuf.len = static_cast<ULONG>(bufferSize);
        wsaOverlapped.hEvent = ::WSACreateEvent();
        if (nullptr == wsaOverlapped.hEvent)
        {
            co_await print("recv failed: create event error");
            co_return false;
        }

        auto errorCode = ::WSARecv(socket, &wsaBuf, 1, nullptr, &flags, &wsaOverlapped, nullptr);
        if ((SOCKET_ERROR == errorCode) && (WSA_IO_PENDING != (errorCode = ::WSAGetLastError())))
        {
            co_await print(co_await lastErrorMessage(errorCode));
            co_return false;
        }

        while (WSA_WAIT_EVENT_0 != ::WSAWaitForMultipleEvents(1, &wsaOverlapped.hEvent, true, 0, false))
            co_switch;

        ::WSAGetOverlappedResult(socket, &wsaOverlapped, &recvBytes, false, &flags);
        ::WSACloseEvent(wsaOverlapped.hEvent);

        co_return recvBytes;
    }

    using endpoint_t = EndPoint;
}

#endif // !ASYNCIO_H