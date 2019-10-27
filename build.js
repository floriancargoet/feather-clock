#!/usr/bin/env node
const { execSync } = require("child_process");

const BOARD_FQBN = "adafruit:samd:adafruit_feather_m0";

// 1 - Find board
const ports = JSON.parse(execSync("arduino-cli board list --format json"));
const port = ports.find(p => p.boards && p.boards.find(b => b.FQBN === BOARD_FQBN));

if (!port) {
    console.error("Board not found");
    process.exit(1);
}

// 2 - Compile, keep stdout in terminal
execSync(`arduino-cli compile --fqbn ${BOARD_FQBN} .`, { stdio: "inherit" });

// 3 - Kill current serial connection
try {
    execSync("screen -XS arduino kill", { stdio: "inherit" });
} catch (e) {}

// 4 - Upload, keep stdout in terminal
execSync(`arduino-cli upload -p ${port.address} --fqbn ${BOARD_FQBN} .`, { stdio: "inherit" });
