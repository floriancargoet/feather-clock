#!/usr/bin/env node
const { execSync } = require("child_process");

const BOARD_FQBN = "adafruit:samd:adafruit_feather_m0";

// 1 - Compile, keep stdout in terminal
console.log("🚧 Compiling…");
execSync(`arduino-cli compile --fqbn ${BOARD_FQBN} .`, { stdio: "inherit" });

// 2 - Find board
console.log("👀 Scanning for boards…");
const ports = JSON.parse(execSync("arduino-cli board list --format json"));
const port = ports.find(p => p.boards && p.boards.find(b => b.FQBN === BOARD_FQBN));

if (!port) {
    console.error("❌ Board not found");
    process.exit(1);
}
else {
    console.log(`✅ Board found: ${port.boards[0].name} on ${port.address}`);
}

// 3 - Kill current serial connection
try {
    console.log("✂️ Closing previous serial connection…");
    execSync("screen -XS arduino kill", { stdio: "inherit" });
} catch (e) {}

// 4 - Upload, keep stdout in terminal

console.log("🚀 Uploading…");
execSync(`arduino-cli upload -p ${port.address} --fqbn ${BOARD_FQBN} .`, { stdio: "inherit" });
