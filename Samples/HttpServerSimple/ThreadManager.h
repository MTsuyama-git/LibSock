#pragma once

#include <HttpHeader.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>

namespace fs = std::filesystem;

template <
    size_t TThreadCount,
    size_t TResponseBufferSize,
    size_t TWebSocketBase64BufferSize>
class ThreadManager
{
public:

    struct ThreadParam
    {
        int          descriptor;
        request_type requestType;
        fs::path     requestPath;
        std::string  secWebSocketKey;
    };

    ThreadManager() noexcept
    {
        for (auto& param : m_ThreadParams)
        {
            param = {
                ThreadState::Idle,
                {
                        -1,
                        request_type::UNKNOWN,
                        "", "",
                        }
            };
        }
    }

    ~ThreadManager() noexcept
    {
        this->Join();
    }

    void StartThread(const std::function<void(
                         const ThreadParam&,
                         const std::span<char>&,
                         const std::span<char>&)>& pFunc) noexcept
    {
        m_StopFlag = false;

        for (auto& thread : m_Threads)
        {
            auto index = std::distance(m_Threads.begin(), &thread);
            thread     = std::thread(
                &ThreadManager::ThreadFunc, this, std::cref(pFunc), &m_ThreadParams[index]);
        }
    }

    void SetParam(const ThreadParam& param) noexcept
    {
        auto* const pParamBegin = m_ThreadParams.begin();
        auto* const pParamEnd   = m_ThreadParams.end();
        auto*       pParam      = pParamEnd;
        do
        {
            m_Mutex.lock();
            pParam =
                std::find_if(pParamBegin, pParamEnd, [](const InternalThreadParam& param) -> bool {
                    return param.state == ThreadState::Idle;
                });
            if (pParamEnd <= pParam)
            {
                m_Mutex.unlock();
            }
        } while (pParamEnd <= pParam);

        pParam->param = param;
        pParam->state             = ThreadState::Triggered;
        m_Mutex.unlock();
    }

    void Join() noexcept
    {
        m_StopFlag = true;

        for (auto& thread : m_Threads)
        {
            if (!thread.joinable())
            {
                continue;
            }
            thread.join();
        }
    }

    static constexpr auto ThreadCount               = TThreadCount;
    static constexpr auto ResponseBufferSize        = TResponseBufferSize;
    static constexpr auto WebSocketBase64BufferSize = TWebSocketBase64BufferSize;

private:

    enum class ThreadState : uint8_t
    {
        Idle,
        Triggered,
        Processing,
    };

    struct InternalThreadParam
    {
        ThreadState state;
        ThreadParam param;
    };

    std::mutex                                   m_Mutex;

    bool                                         m_StopFlag;

    std::array<std::thread, ThreadCount>         m_Threads;
    std::array<InternalThreadParam, ThreadCount> m_ThreadParams;

    void                                         ThreadFunc(
                                                const std::function<void(
            const ThreadParam&,
            const std::span<char>&,
            const std::span<char>&)>& pFunc,
                                                InternalThreadParam*                                pParam) noexcept
    {
        auto* responseBuffer     = new char[ResponseBufferSize]();
        auto* websocketBase64Buffer = new char[WebSocketBase64BufferSize];

        while (!m_StopFlag)
        {
            if (pParam->state == ThreadState::Idle)
            {
                continue;
            }
            pParam->state = ThreadState::Processing;
            pFunc(
                pParam->param,
                {responseBuffer, ResponseBufferSize},
                {websocketBase64Buffer, WebSocketBase64BufferSize});
            pParam->state = ThreadState::Idle;
        }

        delete[] (responseBuffer);
        delete[] (websocketBase64Buffer);
    }
};
