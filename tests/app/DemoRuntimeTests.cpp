#include "app/DemoRuntimeController.h"

#include <atomic>
#include <chrono>
#include <iostream>

namespace
{
int fail(int code, const char* message)
{
    std::cerr << message << '\n';
    return code;
}
}

int main()
{
    const auto root = juce::File("/tmp")
        .getNonexistentChildFile("c2paseq-demo-runtime", {}, true);
    const auto executable = root.getChildFile("Demo.app/Contents/MacOS/Demo");
    const auto paths = c2paseq::DemoRuntimePaths::fromExecutable(executable);
    if (paths.setupScript != root.getChildFile(
            "Demo.app/Contents/Resources/RuntimeSetup/setup_audiowmark.sh"))
        return fail(1, "setup script did not resolve inside the app bundle");
    if (paths.serverScript != root.getChildFile(
            "Demo.app/Contents/Resources/RecoveryDemo/server.py"))
        return fail(2, "recovery server did not resolve inside the app bundle");

    c2paseq::DemoRuntimeController controller({
        root.getChildFile("missing-setup.sh"), root.getChildFile("missing-server.py"),
        root.getChildFile("missing-audiowmark")
    });
    if (controller.audioWMarkAvailable() || controller.installationInProgress()
        || controller.recoveryDemoRunning())
        return fail(3, "missing runtime reported an active state");
    if (controller.installAudioWMark({}).wasOk())
        return fail(4, "missing installer was accepted");
    if (controller.launchRecoveryDemo({}).wasOk())
        return fail(5, "recovery demo launched without AudioWMark");

    const auto fixtureRoot = root.getChildFile("fixture");
    const auto fixtureServer = fixtureRoot.getChildFile("server.py");
    const auto fixtureAudioWMark = fixtureRoot.getChildFile("audiowmark");
    const auto rootResult = root.createDirectory();
    if (rootResult.failed())
    {
        std::cerr << root.getFullPathName() << ": " << rootResult.getErrorMessage() << '\n';
        return fail(6, "could not create recovery fixture root");
    }
    if (! fixtureRoot.createDirectory().wasOk())
        return fail(6, "could not create recovery fixture directory");
    if (! fixtureAudioWMark.replaceWithText("fixture"))
        return fail(6, "could not create recovery fixture executable");
    if (! fixtureServer.replaceWithText(
            "import argparse\n"
            "from http.server import BaseHTTPRequestHandler, HTTPServer\n"
            "parser = argparse.ArgumentParser()\n"
            "parser.add_argument('--port', type=int, required=True)\n"
            "args = parser.parse_args()\n"
            "class Handler(BaseHTTPRequestHandler):\n"
            "    def do_GET(self):\n"
            "        if self.path == '/services/supportedAlgorithms':\n"
            "            body = b'{\\\"algorithms\\\": [\\\"io.github.jeremybboy.audiowmark.1\\\"]}'\n"
            "        elif self.path == '/derivatives/presets':\n"
            "            body = b'{\\\"presets\\\": [{\\\"id\\\": \\\"mp3-64k-lowpass-12k\\\"}]}'\n"
            "        else:\n"
            "            self.send_response(404)\n"
            "            self.end_headers()\n"
            "            return\n"
            "        self.send_response(200)\n"
            "        self.send_header('Content-Length', str(len(body)))\n"
            "        self.end_headers()\n"
            "        self.wfile.write(body)\n"
            "    def log_message(self, format, *args):\n"
            "        pass\n"
            "HTTPServer(('127.0.0.1', args.port), Handler).serve_forever()\n"))
        return fail(6, "could not create recovery fixture server");

    c2paseq::DemoRuntimeController asyncController({
        root.getChildFile("missing-setup.sh"), fixtureServer, fixtureAudioWMark
    });
    const auto startedAt = std::chrono::steady_clock::now();
    if (asyncController.launchRecoveryDemo({}).failed())
        return fail(7, "valid recovery demo startup was rejected");
    const auto launchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    if (launchDuration >= std::chrono::milliseconds(500))
        return fail(8, "recovery demo launch blocked the caller");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
    while (asyncController.recoveryDemoStarting()
           && std::chrono::steady_clock::now() < deadline)
        juce::Thread::sleep(25);
    if (asyncController.recoveryDemoStarting())
        return fail(9, "recovery demo startup did not finish");
    asyncController.stopRecoveryDemo();
    return 0;
}
