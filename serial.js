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

// 2 - Open serial stream
execSync(`screen -S arduino ${port.address} 9600`, { stdio: "inherit" });
