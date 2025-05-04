class BitPacker {
    constructor(maxBits = 128) {
        this.maxBits = maxBits;
        this.buffer = new Uint8Array((maxBits + 7) / 8).fill(0);
        this.bitPosition = maxBits;
        this.totalBits = 0;
    }

    addField(value, bits) {
        if (this.bitPosition < bits || this.totalBits + bits > this.maxBits) {
            return false;
        }
        this.bitPosition -= bits;
        this.totalBits += bits;

        for (let i = 0; i < bits; i++) {
            const bitPos = this.bitPosition + i;
            const byteIdx = Math.floor(bitPos / 8);
            const bitIdx = 7 - (bitPos % 8);
            if (value & (1 << (bits - 1 - i))) {
                this.buffer[byteIdx] |= (1 << bitIdx);
            } else {
                this.buffer[byteIdx] &= ~(1 << bitIdx);
            }
        }
        return true;
    }

    extractField(bits) {
        if (this.bitPosition < bits) return 0;
        this.bitPosition -= bits;

        let value = 0;
        for (let i = 0; i < bits; i++) {
            const bitPos = this.bitPosition + i;
            const byteIdx = Math.floor(bitPos / 8);
            const bitIdx = 7 - (bitPos % 8);
            if (this.buffer[byteIdx] & (1 << bitIdx)) {
                value |= (1 << (bits - 1 - i));
            }
        }
        return value;
    }

    pack7Bit() {
        const outputSize = Math.ceil(this.totalBits / 7);
        const output = new Uint8Array(outputSize);
        for (let i = 0; i < outputSize; i++) {
            let value = 0;
            for (let j = 0; j < 7; j++) {
                const bitPos = (this.maxBits - 1) - (i * 7 + j);
                if (bitPos < (this.maxBits - this.totalBits) || bitPos >= this.maxBits) break;
                const byteIdx = Math.floor(bitPos / 8);
                const bitIdx = 7 - (bitPos % 8);
                if (this.buffer[byteIdx] & (1 << bitIdx)) {
                    value |= (1 << (6 - j));
                }
            }
            output[i] = value & 0x7F;
        }
        return output;
    }

    unpack7Bit(input) {
        this.buffer.fill(0);
        const totalBits = input.length * 7;
        if (totalBits > this.maxBits) return false;
        this.totalBits = totalBits;
        this.bitPosition = this.maxBits;

        for (let i = 0; i < input.length; i++) {
            const value = input[i] & 0x7F;
            for (let j = 0; j < 7; j++) {
                const bitPos = (this.maxBits - 1) - (i * 7 + j);
                if (bitPos < (this.maxBits - totalBits)) break;
                const byteIdx = Math.floor(bitPos / 8);
                const bitIdx = 7 - (bitPos % 8);
                if (value & (1 << (6 - j))) {
                    this.buffer[byteIdx] |= (1 << bitIdx);
                } else {
                    this.buffer[byteIdx] &= ~(1 << bitIdx);
                }
            }
        }
        return true;
    }
}

let midiInput = null;
let midiOutput = null;
const errormodal = document.getElementById('error-modal');
const errortext = document.getElementById('error-text');
const notifymodal = document.getElementById('notify-modal');
const notifytext = document.getElementById('notify-text');
const firmwaremodal = document.getElementById('firmware-modal');
const firmwaretext = document.getElementById('firmware-text');
const errornodevice = 'PicoKeyer not found. Please ensure that the PicoKeyer is plugged in and refresh the page to try again.';

const groupStates = {
    paddle: false,
    straightkey: false,
    normalled: false,
    rgbLED: false
};

function updateGroupVisibility(groupName, visibility) {
    // Update the group's state
    groupStates[groupName] = visibility;

    const rows = document.querySelectorAll('tr[data-group]');
    rows.forEach(row => {
        const groups = row.getAttribute('data-group').split(' ');
        const isVisible = groups.some(group => groupStates[group] === true);
        row.classList.toggle('hidden', !isVisible);
    });
}

function keyModeChange() {
    const keyMode = parseInt(document.getElementById('keyMode').value);

    switch (keyMode) {
        case 0: {
            updateGroupVisibility('paddles', false);
            updateGroupVisibility('straightkey', false);
            updateGroupVisibility('normalled', false);
            updateGroupVisibility('rgbLED', false);
            updateGroupVisibility('output', false);
            break;
        }
        case 1: {
            updateGroupVisibility('paddles', false);
            updateGroupVisibility('straightkey', true);
            ledModeChange();
            gpioOutputModeChange();
            break;
        }
        case 2: {
            updateGroupVisibility('paddles', true);
            updateGroupVisibility('straightkey', false);
            ledModeChange();
            gpioOutputModeChange();
            break;
        }
    }
}

function ledModeChange() {
    const ledMode = parseInt(document.getElementById('ledMode').value);

    switch (ledMode) {
        case 0: {
            updateGroupVisibility('normalled', false);
            updateGroupVisibility('rgbLED', false);
            break;
        }
        case 1: {
            updateGroupVisibility('normalled', true);
            updateGroupVisibility('rgbLED', false);
            break;
        }
        case 2: {
            updateGroupVisibility('normalled', false);
            updateGroupVisibility('rgbLED', true);
            break;
        }
    }
}

function gpioOutputModeChange() {
    const gpioOutputMode = parseInt(document.getElementById('gpioOutputMode').value);

    updateGroupVisibility('output', gpioOutputMode != 0);
}

async function requestMidiAccess() {
    // Ensure the right fields are hidden/displayed
    keyModeChange();

    try {
        const midiAccess = await navigator.requestMIDIAccess({ sysex: true });
        // Populate input
        for (let input of midiAccess.inputs.values()) {
            if (input.name == 'PicoKeyer') {
                midiInput = input;
                break;
            }
        }
        // Populate output
        for (let output of midiAccess.outputs.values()) {
            if (output.name == 'PicoKeyer') {
                midiOutput = output;
                break;
            }
        }

        if (!midiInput || !midiOutput) {
            // PicoKeyer not found
            errortext.textContent = errornodevice;
            openModal(errormodal);
            return;
        }

        midiInput.onmidimessage = handleMidiMessage;

        // Request config
        sendGetVersion();

    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

function encodeConfig(config) {
    const packer = new BitPacker(128);
    packer.addField(config.keyMode & 0x3, 2);
    packer.addField(config.pinMode & 0x3, 2);
    packer.addField(config.ledMode & 0x3, 2);
    packer.addField(config.gpioOutputMode & 0x3, 2);
    packer.addField(config.output & 0x7F, 7);
    packer.addField(config.normalLED & 0x7F, 7);
    packer.addField(config.rgbLED & 0x7F, 7);
    packer.addField(config.ditPaddle & 0x7F, 7);
    packer.addField(config.dahPaddle & 0x7F, 7);
    packer.addField(config.straightKey & 0x7F, 7);
    packer.addField(Math.round(config.wpm * 100), 16);
    packer.addField(config.channel & 0x7F, 7);
    packer.addField(config.note & 0x7F, 7);
    packer.addField(config.volume & 0x7F, 7);
    return packer.pack7Bit();
}

function decodeConfig(data) {
    const packer = new BitPacker(128);
    if (!packer.unpack7Bit(data)) {
        throw new Error('Failed to unpack SysEx data');
    }
    return {
        keyMode: packer.extractField(2),
        pinMode: packer.extractField(2),
        ledMode: packer.extractField(2),
        gpioOutputMode: packer.extractField(2),
        output: packer.extractField(7),
        normalLED: packer.extractField(7),
        rgbLED: packer.extractField(7),
        ditPaddle: packer.extractField(7),
        dahPaddle: packer.extractField(7),
        straightKey: packer.extractField(7),
        wpm: packer.extractField(16) / 100,
        channel: packer.extractField(7),
        note: packer.extractField(7),
        volume: packer.extractField(7)
    };
}

function decodeVersion(data) {
    const packer = new BitPacker(128);
    if (!packer.unpack7Bit(data)) {
        throw new Error('Failed to unpack SysEx data');
    }
    return {
        version: packer.extractField(16)
    };
}


function buildConfig() {
    try {
        const config = {
            keyMode: parseInt(document.getElementById('keyMode').value),
            pinMode: parseInt(document.getElementById('pinMode').value),
            ledMode: parseInt(document.getElementById('ledMode').value),
            gpioOutputMode: parseInt(document.getElementById('gpioOutputMode').value),
            output: parseInt(document.getElementById('output').value),
            normalLED: parseInt(document.getElementById('normalLED').value),
            rgbLED: parseInt(document.getElementById('rgbLED').value),
            ditPaddle: parseInt(document.getElementById('ditPaddle').value),
            dahPaddle: parseInt(document.getElementById('dahPaddle').value),
            straightKey: parseInt(document.getElementById('straightKey').value),
            wpm: parseFloat(document.getElementById('wpm').value),
            channel: parseInt(document.getElementById('channel').value),
            note: parseInt(document.getElementById('note').value),
            volume: parseInt(document.getElementById('volume').value)
        };
        const data = encodeConfig(config);
        return data;
    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

async function sendGetVersion() {
    if (!midiOutput) {
        errortext.textContent = errornodevice;
        openModal(errormodal);
        return;
    }
    try {
        const sysex = [0xF0, 0x7D, 0x00, 0xF7]; // Use [0xF0, 0x00, 0x00, 0x7F, 0x02, 0xF7] for three-byte ID
        midiOutput.send(sysex);
        console.log(`Sent Get Config: ${sysex.map(b => b.toString(16).padStart(2, '0')).join(' ')}\n`);
    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

async function sendGetConfig() {
    if (!midiOutput) {
        errortext.textContent = errornodevice;
        openModal(errormodal);
        return;
    }
    try {
        const sysex = [0xF0, 0x7D, 0x01, 0xF7]; // Use [0xF0, 0x00, 0x00, 0x7F, 0x02, 0xF7] for three-byte ID
        midiOutput.send(sysex);
        console.log(`Sent Get Config: ${sysex.map(b => b.toString(16).padStart(2, '0')).join(' ')}\n`);
    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

async function sendSetConfig() {
    if (!midiOutput) {
        errortext.textContent = errornodevice;
        openModal(errormodal);
        return;
    }
    try {
        const data = buildConfig();
        const sysex = [0xF0, 0x7D, 0x02, ...data, 0xF7]; // Use [0xF0, 0x00, 0x00, 0x7F, 0x01, ...data, 0xF7] for three-byte ID
        midiOutput.send(sysex);
    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

async function sendSaveConfig() {
    if (!midiOutput) {
        errortext.textContent = errornodevice;
        openModal(errormodal);
        return;
    }
    try {
        const data = buildConfig();
        const sysex = [0xF0, 0x7D, 0x03, ...data, 0xF7]; // Use [0xF0, 0x00, 0x00, 0x7F, 0x01, ...data, 0xF7] for three-byte ID
        midiOutput.send(sysex);
    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

async function sendReboot() {
    if (!midiOutput) {
        errortext.textContent = errornodevice;
        openModal(errormodal);
        return;
    }
    try {
        const sysex = [0xF0, 0x7D, 0x04, 0xF7];
        midiOutput.send(sysex);
    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

async function sendUpdateFirmware() {
    if (!midiOutput) {
        errortext.textContent = errornodevice;
        openModal(errormodal);
        return;
    }
    try {
        const sysex = [0xF0, 0x7D, 0x05, 0xF7];
        midiOutput.send(sysex);
    } catch (error) {
        errortext.textContent = error;
        openModal(errormodal);
    }
}

function handleMidiMessage(event) {
    const data = event.data;
    if (data.length <= 3) handleNote(event);
    if (data[0] !== 0xF0 || data[data.length - 1] !== 0xF7) return;
    if (data[1] !== 0x7D) return; // Update to check [0x00, 0x00, 0x7F] for three-byte ID
    const command = data[2]; // Adjust to data[4] for three-byte ID
    if (command === 0x0) {
        const versionData = data.slice(3, -1); // Adjust to slice(5, 14) for three-byte ID
        const version = decodeVersion(versionData);

        if (version.version != 0x2) {
            firmwaretext.innerHTML = 'Download the latest PicoKeyer firmware <a target="_blank" href="https://github.com/bontebok/PicoKeyer/releases">\
                here.</a> Once you have the downloaded the firmware, click the <b>Update Firmware</b> button below.<br><br> \
                A new drive letter will appear named <b>RPI-RP2</b> containing files INDEX.HTM and INFO_UF2.TXT. Copy the <b>PicoKeyer.uf2</b>\
                file onto the RPI-RP2 drive.\n\n The PicoKeyer will automatically restart.<br><br>When complete, refresh this page to continue.';
            openModal(firmwaremodal);
            return;
        }
        else {
            sendGetConfig();
        }
    }
    if (command === 0x1) {
        try {
            const configData = data.slice(3, -1); // Adjust to slice(5, 14) for three-byte ID
            const config = decodeConfig(configData);
            // Update form fields
            document.getElementById('keyMode').value = config.keyMode;
            document.getElementById('pinMode').value = config.pinMode;
            document.getElementById('ledMode').value = config.ledMode;
            document.getElementById('gpioOutputMode').value = config.gpioOutputMode;
            document.getElementById('output').value = config.output;
            document.getElementById('normalLED').value = config.normalLED;
            document.getElementById('rgbLED').value = config.rgbLED;
            document.getElementById('ditPaddle').value = config.ditPaddle;
            document.getElementById('dahPaddle').value = config.dahPaddle;
            document.getElementById('straightKey').value = config.straightKey;
            document.getElementById('wpm').value = config.wpm.toFixed(2);
            document.getElementById('channel').value = config.channel;
            document.getElementById('note').value = config.note;
            document.getElementById('volume').value = config.volume;
            // Ensure the right fields are hidden/displayed
            document.getElementById('main').classList.remove('hidden');
            keyModeChange();
            resizeCanvas();
        } catch (error) {
            errortext.textContent = error;
            openModal(errormodal);
        }
    }
}

window.onload = requestMidiAccess();