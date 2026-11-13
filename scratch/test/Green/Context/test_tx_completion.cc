#include "vanetza_integration/context/tx_bridge/tx_completion.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << name << '\n';
    return condition;
}

} // namespace

int main()
{
    bool passed = true;

    // A primeira conclusão de sucesso deve ser aceite.
    unsigned successCalls = 0;
    ns3::TxCompletionResult successResult{};

    ns3::TxCompletion successCompletion(
        [&](const ns3::TxCompletionResult& result)
        {
            ++successCalls;
            successResult = result;
        });

    passed &= Check(
        !successCompletion.IsCompleted(),
        "COMPLETION_STARTS_PENDING");

    const bool firstSuccess = successCompletion.Complete({
        ns3::TxCompletionStatus::Success,
        1234,
        "SendTo OK"
    });

    passed &= Check(
        firstSuccess,
        "SUCCESS_FIRST_COMPLETION_ACCEPTED");

    passed &= Check(
        successCalls == 1,
        "SUCCESS_CALLBACK_INVOKED_ONCE");

    // Uma falha tardia não pode substituir o sucesso já confirmado.
    const bool lateFailure = successCompletion.Complete({
        ns3::TxCompletionStatus::Failure,
        5678,
        "late failure"
    });

    passed &= Check(
        !lateFailure &&
            successCalls == 1,
        "LATE_FAILURE_IGNORED");

    passed &= Check(
        successResult.status ==
                ns3::TxCompletionStatus::Success &&
            successResult.completedAtSimUs == 1234 &&
            successResult.reason == "SendTo OK",
        "FIRST_SUCCESS_RESULT_PRESERVED");

    // O mesmo contrato deve funcionar quando a primeira conclusão é falha.
    unsigned failureCalls = 0;
    ns3::TxCompletionResult failureResult{};

    ns3::TxCompletion failureCompletion(
        [&](const ns3::TxCompletionResult& result)
        {
            ++failureCalls;
            failureResult = result;
        });

    const bool firstFailure = failureCompletion.Complete({
        ns3::TxCompletionStatus::Failure,
        2500,
        "DCC lifetime expired"
    });

    const bool lateSuccess = failureCompletion.Complete({
        ns3::TxCompletionStatus::Success,
        3000,
        "SendTo OK"
    });

    passed &= Check(
        firstFailure && !lateSuccess &&
            failureCalls == 1,
        "FAILURE_CALLBACK_INVOKED_ONCE");

    passed &= Check(
        failureResult.status ==
                ns3::TxCompletionStatus::Failure &&
            failureResult.completedAtSimUs == 2500 &&
            failureResult.reason == "DCC lifetime expired",
        "FIRST_FAILURE_RESULT_PRESERVED");

    // Concorrência: só uma chamada pode ganhar a conclusão terminal.
    std::atomic<unsigned> concurrentCallbackCalls{0};
    std::atomic<unsigned> acceptedCompletions{0};

    auto concurrentCompletion =
        std::make_shared<ns3::TxCompletion>(
            [&](const ns3::TxCompletionResult&)
            {
                concurrentCallbackCalls.fetch_add(
                    1, std::memory_order_relaxed);
            });

    std::vector<std::thread> workers;

    for (unsigned i = 0; i < 16; ++i)
    {
        workers.emplace_back(
            [concurrentCompletion, &acceptedCompletions, i]()
            {
                const bool accepted =
                    concurrentCompletion->Complete({
                        (i % 2 == 0)
                            ? ns3::TxCompletionStatus::Success
                            : ns3::TxCompletionStatus::Failure,
                        static_cast<std::int64_t>(i),
                        "concurrent terminal result"
                    });

                if (accepted)
                {
                    acceptedCompletions.fetch_add(
                        1, std::memory_order_relaxed);
                }
            });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    passed &= Check(
        acceptedCompletions.load() == 1,
        "CONCURRENT_COMPLETION_ACCEPTED_ONCE");

    passed &= Check(
        concurrentCallbackCalls.load() == 1 &&
            concurrentCompletion->IsCompleted(),
        "CONCURRENT_CALLBACK_INVOKED_ONCE");

    if (!passed)
    {
        std::cout << "TX_COMPLETION=FAIL\n";
        return 1;
    }

    std::cout << "TX_COMPLETION=PASS\n";
    return 0;
}
