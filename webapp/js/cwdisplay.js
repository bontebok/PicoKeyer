const canvas = document.getElementById('morseCanvas');
const ctx = canvas.getContext('2d');

// Function to set canvas size to match parent container
function resizeCanvas() {
    const parent = canvas.parentElement;
    const parentStyle = window.getComputedStyle(parent);
    const width = parseFloat(parentStyle.width) -
        (parseFloat(parentStyle.paddingLeft) +
            parseFloat(parentStyle.paddingRight) +
            parseFloat(parentStyle.borderLeftWidth) +
            parseFloat(parentStyle.borderRightWidth));
    const height = parseFloat(parentStyle.height) -
        (parseFloat(parentStyle.paddingTop) +
            parseFloat(parentStyle.paddingBottom) +
            parseFloat(parentStyle.borderTopWidth) +
            parseFloat(parentStyle.borderBottomWidth));
    canvas.width = width;
    canvas.height = height;
    // Redraw background
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
}

// Initial size and resize listener
resizeCanvas();
window.addEventListener('resize', resizeCanvas);

// Drawing settings
let xOffset = 0; // Tracks scroll position
let scrollSpeed; // Pixels per frame, scaled to canvas
let isDrawing = false; // Tracks signal state
let lineY; // Vertical position, dynamic
let lineWidth; // Thickness, dynamic
const signalSegments = []; // Stores {start, end} of signal "on" periods
let currentSegment = null; // Tracks active segment

// Update dynamic settings based on canvas size
function updateSettings() {
    scrollSpeed = canvas.width * 0.004; // Scale scroll speed
    lineY = canvas.height / 2; // Center vertically
    lineWidth = canvas.height * 0.05; // 5% of canvas height
}

// Draw the Morse code signal from segments
function drawSignal() {
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.beginPath();
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = lineWidth;
    ctx.lineCap = 'square';

    // Draw all visible segments
    signalSegments.forEach(segment => {
        const startX = canvas.width - (xOffset - segment.start);
        const endX = segment.end !== null ? canvas.width - (xOffset - segment.end) : canvas.width;
        // Only draw if segment is visible
        if (startX < canvas.width && endX > 0) {
            ctx.moveTo(Math.max(0, startX), lineY);
            ctx.lineTo(Math.min(canvas.width, endX), lineY);
        }
    });

    // Draw the current segment if active
    if (currentSegment) {
        const startX = canvas.width - (xOffset - currentSegment.start);
        ctx.moveTo(Math.max(0, startX), lineY);
        ctx.lineTo(canvas.width, lineY);
    }

    ctx.stroke();
}

// Animation loop
function animate() {
    // Update settings in case canvas resized
    updateSettings();

    // Update signal segments
    if (isDrawing && !currentSegment) {
        // Start a new segment
        currentSegment = { start: xOffset, end: null };
    } else if (!isDrawing && currentSegment) {
        // End the current segment
        currentSegment.end = xOffset;
        signalSegments.push(currentSegment);
        currentSegment = null;
    } else if (isDrawing && currentSegment) {
        // Update current segment's end
        currentSegment.end = xOffset;
    }

    // Update xOffset
    xOffset += scrollSpeed;

    // Clean up old segments
    while (signalSegments.length > 0 && (xOffset - signalSegments[0].end) > canvas.width) {
        signalSegments.shift();
    }

    // Draw the signal
    drawSignal();

    requestAnimationFrame(animate);
}

// Start animation
animate();