let audioContext = new (window.AudioContext || window.webkitAudioContext)();
let oscillators = {};
let enableSound = false;

const togglesound = document.getElementById('togglesound');

togglesound.addEventListener('change', function () {
    if (this.checked) {
        enableSound = true;
    } else {
        enableSound = false;
    }
});

function handleNote(message) {
    const [command, note, velocity] = message.data;
    const noteOn = command >= 144 && command < 160;
    const noteOff = command >= 128 && command < 144;

    if (noteOn && velocity > 0) {
        isDrawing = true;
        if (enableSound) playNote(note, velocity);
    } else if (noteOff || (noteOn && velocity === 0)) {
        isDrawing = false;
        if (enableSound) stopNote(note);
    }
}

function playNote(note, velocity) {
    if (oscillators[note]) return;

    const oscillator = audioContext.createOscillator();
    const gainNode = audioContext.createGain();

    oscillator.type = 'sine';
    oscillator.frequency.setValueAtTime(
        440 * Math.pow(2, (note - 69) / 12),
        audioContext.currentTime
    );

    // Start with very low gain and use linear ramp for smoother fade-in
    gainNode.gain.setValueAtTime(0.0001, audioContext.currentTime);
    gainNode.gain.linearRampToValueAtTime(
        velocity / 127,
        audioContext.currentTime + 0.02 // 20ms fade-in for smoother start
    );

    oscillator.connect(gainNode);
    gainNode.connect(audioContext.destination);

    // Slight delay to ensure gain is set before starting
    oscillator.start(audioContext.currentTime + 0.001);
    oscillators[note] = { oscillator, gainNode };

}

function stopNote(note) {
    if (!oscillators[note]) return;

    const { oscillator, gainNode } = oscillators[note];
    // Ramp down gain to prevent popping
    gainNode.gain.cancelScheduledValues(audioContext.currentTime);
    gainNode.gain.setValueAtTime(gainNode.gain.value, audioContext.currentTime);
    gainNode.gain.exponentialRampToValueAtTime(
        0.0001, // Very small value instead of 0 to avoid abrupt cut
        audioContext.currentTime + 0.05 // 50ms fade-out
    );
    oscillator.stop(audioContext.currentTime + 0.06); // Stop slightly after fade
    delete oscillators[note];

    // Remove visual indicator
    const noteElement = document.getElementById(`note-${note}`);
    if (noteElement) {
        noteElement.classList.remove('active');
    }
}