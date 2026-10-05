#include "DemoRuntimeController.h"

#include <array>

namespace c2paseq
{
namespace
{
bool endpointContains(int port, const juce::String& path, const juce::String& marker)
{
    juce::StreamingSocket socket;
    if (! socket.connect("127.0.0.1", port, 200))
        return false;

    const juce::String request(
        "GET " + path + " HTTP/1.0\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n");
    if (socket.write(request.toRawUTF8(),
                     static_cast<int>(request.getNumBytesAsUTF8())) < 0)
        return false;

    juce::MemoryOutputStream response;
    std::array<char, 2048> buffer {};
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 500.0;
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        const auto ready = socket.waitUntilReady(true, 50);
        if (ready < 0)
            return false;
        if (ready == 0)
            continue;
        const auto count = socket.read(buffer.data(), static_cast<int>(buffer.size()), false);
        if (count <= 0)
            return false;
        response.write(buffer.data(), static_cast<std::size_t>(count));
        if (response.toString().contains(marker))
            return true;
    }
    return false;
}

bool expectedRecoveryServiceIsReady(int port)
{
    return endpointContains(port, "/services/supportedAlgorithms",
                            "io.github.jeremybboy.audiowmark.1")
        && endpointContains(port, "/derivatives/presets", "mp3-64k-lowpass-12k");
}
}

DemoRuntimePaths DemoRuntimePaths::fromExecutable(const juce::File& executable)
{
    const auto contents = executable.getParentDirectory().getParentDirectory();
    const auto resources = contents.getChildFile("Resources");
    const auto applicationSupport = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory).getChildFile("Application Support")
        .getChildFile("C2PA Creative Sequencer");
    return {
        resources.getChildFile("RuntimeSetup/setup_audiowmark.sh"),
        resources.getChildFile("RecoveryDemo/server.py"),
        applicationSupport.getChildFile("AudioWMark/bin/audiowmark")
    };
}

DemoRuntimeController::DemoRuntimeController(DemoRuntimePaths pathsToUse)
    : paths(std::move(pathsToUse))
{
}

DemoRuntimeController::~DemoRuntimeController()
{
    shuttingDown.store(true);
    installationProcess.kill();
    if (installationThread.joinable())
        installationThread.join();
    stopRecoveryDemo();
}

bool DemoRuntimeController::audioWMarkAvailable() const
{
    return paths.audioWMarkExecutable.existsAsFile();
}

bool DemoRuntimeController::installationInProgress() const noexcept
{
    return installing.load();
}

bool DemoRuntimeController::recoveryDemoStarting() const noexcept
{
    return startingRecovery.load();
}

bool DemoRuntimeController::recoveryDemoRunning() const
{
    return recoveryReady.load();
}

juce::String DemoRuntimeController::recoveryDemoUrl() const
{
    return "http://127.0.0.1:" + juce::String(recoveryPort.load());
}

juce::Result DemoRuntimeController::installAudioWMark(Completion completion)
{
    if (audioWMarkAvailable())
        return juce::Result::fail("AudioWMark is already installed");
    if (installing.exchange(true))
        return juce::Result::fail("AudioWMark installation is already running");
    if (! paths.setupScript.existsAsFile())
    {
        installing.store(false);
        return juce::Result::fail("The bundled AudioWMark installer is missing");
    }
    if (installationThread.joinable())
        installationThread.join();

    const juce::StringArray command { "/bin/sh", paths.setupScript.getFullPathName() };
    if (! installationProcess.start(command))
    {
        installing.store(false);
        return juce::Result::fail("Could not start the AudioWMark installer");
    }

    installationThread = std::thread([this, finished = std::move(completion)]() mutable
    {
        juce::MemoryOutputStream output;
        std::array<char, 4096> buffer {};
        while (installationProcess.isRunning() && ! shuttingDown.load())
        {
            const auto count = installationProcess.readProcessOutput(
                buffer.data(), static_cast<int>(buffer.size()));
            if (count > 0)
                output.write(buffer.data(), static_cast<std::size_t>(count));
            else
                juce::Thread::sleep(50);
        }
        while (const auto count = installationProcess.readProcessOutput(
                   buffer.data(), static_cast<int>(buffer.size())))
            output.write(buffer.data(), static_cast<std::size_t>(count));

        const auto exitCode = installationProcess.getExitCode();
        installing.store(false);
        if (shuttingDown.load())
            return;

        auto result = exitCode == 0 && audioWMarkAvailable()
            ? juce::Result::ok()
            : juce::Result::fail("AudioWMark installation failed"
                + (output.getDataSize() > 0
                    ? ": " + output.toString().trim().substring(0, 1200)
                    : juce::String(" (exit code ") + juce::String(exitCode) + ")"));
        juce::MessageManager::callAsync(
            [deliver = std::move(finished), result]() mutable
            {
                if (deliver)
                    deliver(result);
            });
    });
    return juce::Result::ok();
}

juce::Result DemoRuntimeController::launchRecoveryDemo(Completion completion)
{
    if (! audioWMarkAvailable())
        return juce::Result::fail("Install AudioWMark before launching the recovery demo");
    if (! paths.serverScript.existsAsFile())
        return juce::Result::fail("The bundled recovery demo server is missing");
    if (startingRecovery.exchange(true))
        return juce::Result::fail("The recovery demo is already starting");

    if (recoveryThread.joinable())
        recoveryThread.join();
    stopRecoveryRequested.store(false);
    recoveryReady.store(false);
    recoveryThread = std::thread([this, finished = std::move(completion)]() mutable
    {
        constexpr int firstPort = 8787;
        constexpr int lastPort = 8797;
        auto result = juce::Result::fail(
            "No free recovery-demo port was found from 8787 to 8797");

        for (int port = firstPort;
             port <= lastPort && ! stopRecoveryRequested.load() && ! shuttingDown.load();
             ++port)
        {
            juce::StreamingSocket probe;
            if (probe.connect("127.0.0.1", port, 100))
            {
                if (expectedRecoveryServiceIsReady(port))
                {
                    recoveryPort.store(port);
                    result = juce::Result::ok();
                    break;
                }
                continue;
            }

            const juce::StringArray command { "/usr/bin/env", "PYTHONDONTWRITEBYTECODE=1",
                                              "python3", "-u",
                                              paths.serverScript.getFullPathName(),
                                              "--port", juce::String(port) };
            {
                const std::scoped_lock lock(recoveryProcessMutex);
                if (! recoveryProcess.start(command, 0))
                {
                    result = juce::Result::fail(
                        "Could not start Python 3 for the recovery demo");
                    break;
                }
            }

            const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000.0;
            while (juce::Time::getMillisecondCounterHiRes() < deadline
                   && ! stopRecoveryRequested.load() && ! shuttingDown.load())
            {
                {
                    const std::scoped_lock lock(recoveryProcessMutex);
                    if (! recoveryProcess.isRunning())
                        break;
                }

                if (expectedRecoveryServiceIsReady(port))
                {
                    recoveryPort.store(port);
                    result = juce::Result::ok();
                    break;
                }
                juce::Thread::sleep(50);
            }
            if (result.wasOk())
                break;

            {
                const std::scoped_lock lock(recoveryProcessMutex);
                if (recoveryProcess.isRunning())
                    recoveryProcess.kill();
            }
        }

        if (stopRecoveryRequested.load() || shuttingDown.load())
            result = juce::Result::fail("Recovery demo startup was cancelled");
        recoveryReady.store(result.wasOk());
        startingRecovery.store(false);
        if (shuttingDown.load())
            return;
        if (finished)
            juce::MessageManager::callAsync(
                [deliver = std::move(finished), result]() mutable
                {
                    deliver(result);
                });
    });
    return juce::Result::ok();
}

void DemoRuntimeController::stopRecoveryDemo()
{
    stopRecoveryRequested.store(true);
    recoveryReady.store(false);
    {
        const std::scoped_lock lock(recoveryProcessMutex);
        if (recoveryProcess.isRunning())
        {
            recoveryProcess.kill();
            recoveryProcess.waitForProcessToFinish(1000);
        }
    }
    if (recoveryThread.joinable())
        recoveryThread.join();
    startingRecovery.store(false);
    recoveryPort.store(8787);
}
}
