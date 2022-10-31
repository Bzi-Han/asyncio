#include "HexDump.h"

#include "asyncio.h"

#include <mutex>
#include <unordered_map>
#include <queue>
#include <string>

using scheduler = coroutine::STScheduler;

constexpr auto MAX_BUFFER_SIZE = 10 * 1024 * 1024;

std::mutex g_takeClientMutex;
std::unordered_map<size_t, asyncio::endpoint_t> g_clients;
std::queue<asyncio::endpoint_t> g_readable;
std::queue<std::pair<size_t, std::vector<char>>> g_datas;

coroutine::task<> echoHello(const asyncio::endpoint_t &endpoint)
{
    co_await asyncio::send(endpoint.socket, "Welcome to the asyncio!");
}

coroutine::task<> serverAccpet()
{
    SOCKET fd{};
    fd_set checkConnection{};

    co_finally
    {
        if (0 != fd && INVALID_SOCKET != fd)
            co_await asyncio::closesocket(fd);
    };

    fd = co_await asyncio::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == fd)
        co_return co_await asyncio::print("create socket failed");

    if (!co_await asyncio::bind(fd, "127.0.0.1", 23333))
        co_return co_await asyncio::print("bind server config error");

    if (!co_await asyncio::listen(fd, SOMAXCONN))
        co_return co_await asyncio::print("listening the unconnected socket failed");

    for (;;)
    {
        FD_ZERO(&checkConnection);
        FD_SET(fd, &checkConnection);

        if (1 > co_await asyncio::select(&checkConnection, nullptr, nullptr))
            continue;

        asyncio::endpoint_t endpoint = co_await asyncio::accept(fd);
        co_await asyncio::print("[get connect] " + std::to_string(endpoint.id) + " " + endpoint.ip + ":" + std::to_string(endpoint.port));
        co_await echoHello(endpoint);
        if constexpr (std::is_same_v<scheduler, coroutine::STScheduler>)
            g_clients.emplace(endpoint.id, std::move(endpoint));
        else
        {
            std::unique_lock<std::mutex> locker(g_takeClientMutex);

            g_clients.emplace(endpoint.id, std::move(endpoint));
        }
    }
}

coroutine::task<> serverWaitRead()
{
    fd_set checkReadable{};

    for (;;)
    {
        while (g_clients.empty())
            co_switch;

        FD_ZERO(&checkReadable);
        for (const auto &[_, endpoint] : g_clients)
            FD_SET(endpoint.socket, &checkReadable);

        if (1 > co_await asyncio::select(&checkReadable, nullptr, nullptr))
            continue;

        for (const auto &[_, endpoint] : g_clients)
        {
            if (FD_ISSET(endpoint.socket, &checkReadable))
                g_readable.push(endpoint);
        }

        co_switch;
    }
}

coroutine::task<> serverRead()
{
    auto buffer = new char[MAX_BUFFER_SIZE];

    co_finally
    {
        delete[] buffer;
        co_return;
    };

    for (;;)
    {
        while (g_readable.empty())
            co_switch;

        auto endpoint = g_readable.front();
        g_readable.pop();

        size_t readBytes = co_await asyncio::recv(endpoint.socket, buffer, MAX_BUFFER_SIZE);
        if (0 == readBytes)
        {
            g_clients.erase(endpoint.id);
            co_await asyncio::print("[lose connect] " + std::to_string(endpoint.id) + " " + endpoint.ip + ":" + std::to_string(endpoint.port));
            continue;
        }

        g_datas.emplace(endpoint.id, std::vector(buffer, buffer + readBytes));
    }
}

coroutine::task<> serverProcess()
{
    for (;;)
    {
        while (g_datas.empty())
            co_switch;

        auto [id, data] = g_datas.front();
        g_datas.pop();
        auto endpoint = g_clients[id];

        co_await asyncio::print("[data recvice] " + std::to_string(endpoint.id) + " " + endpoint.ip + ":" + std::to_string(endpoint.port));
        HexDump::print(data.data(), data.size());
        co_await asyncio::print("size:" + std::to_string(data.size()));
    }
}

int main()
{
    WSADATA wsaData{};

    auto errorCode = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (errorCode)
    {
        std::cout << "initialization failed:" << errorCode << std::endl;
        return 1;
    }

    scheduler::runs(serverAccpet, serverWaitRead, serverRead, serverProcess);

    getchar();

    ::WSACleanup();
}