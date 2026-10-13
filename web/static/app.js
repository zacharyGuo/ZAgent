// ZAgent Web Client

const API_BASE = '/api';
let currentSessionId = null;

// DOM Elements
const messageInput = document.getElementById('messageInput');
const sendButton = document.getElementById('sendButton');
const chatMessages = document.getElementById('chatMessages');
const sessionList = document.getElementById('sessionList');
const newSessionButton = document.getElementById('newSessionButton');

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    // Create default session on load
    createSession();

    // Event listeners
    sendButton.addEventListener('click', sendMessage);
    messageInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') sendMessage();
    });
    newSessionButton.addEventListener('click', createSession);
});

// Create a new session
async function createSession() {
    try {
        const response = await fetch(`${API_BASE}/sessions`, { method: 'GET' });
        if (response.ok) {
            const data = await response.json();
            // Create new session with unique ID
            currentSessionId = generateUUID();
            updateSessionList();
            displayMessage('System', 'New session created. Type a message to start.', 'system');
        }
    } catch (error) {
        console.error('Failed to create session:', error);
    }
}

// Send a message to the server
async function sendMessage() {
    const message = messageInput.value.trim();
    if (!message) return;

    // Display user message
    displayMessage('User', message, 'user');
    messageInput.value = '';

    // Show loading indicator
    const loadingId = displayMessage('ZAgent', 'Thinking...', 'assistant');

    try {
        const response = await fetch(`${API_BASE}/chat`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                sessionId: currentSessionId,
                message: message
            })
        });

        // Remove loading message
        removeMessage(loadingId);

        if (response.ok) {
            const data = await response.json();
            displayMessage('ZAgent', data.reply, 'assistant');
        } else {
            const errorData = await response.json();
            displayMessage('Error', errorData.error || 'Unknown error', 'error');
        }
    } catch (error) {
        removeMessage(loadingId);
        displayMessage('Error', 'Failed to connect to server', 'error');
    }
}

// Display a message in the chat
function displayMessage(sender, message, type = 'assistant') {
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${type}`;

    const senderDiv = document.createElement('div');
    senderDiv.className = 'message-sender';
    senderDiv.textContent = sender;

    const contentDiv = document.createElement('div');
    contentDiv.className = 'message-content';
    contentDiv.textContent = message;

    messageDiv.appendChild(senderDiv);
    messageDiv.appendChild(contentDiv);

    chatMessages.appendChild(messageDiv);

    // Scroll to bottom
    chatMessages.scrollTop = chatMessages.scrollHeight;

    return messageDiv.id;
}

// Remove a message by ID
function removeMessage(id) {
    const element = document.getElementById(id);
    if (element) element.remove();
}

// Generate UUID
function generateUUID() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
        const r = Math.random() * 16 | 0;
        const v = c === 'x' ? r : (r & 0x3 | 0x8);
        return v.toString(16);
    });
}

// Update session list
async function updateSessionList() {
    try {
        const response = await fetch(`${API_BASE}/sessions`, { method: 'GET' });
        if (response.ok) {
            const data = await response.json();
            sessionList.innerHTML = '';

            for (const sessionId of data.sessions) {
                const div = document.createElement('div');
                div.className = 'session-item';
                div.textContent = sessionId;
                div.addEventListener('click', () => loadSession(sessionId));
                sessionList.appendChild(div);
            }
        }
    } catch (error) {
        console.error('Failed to update session list:', error);
    }
}

// Load a session by ID
function loadSession(sessionId) {
    currentSessionId = sessionId;
    chatMessages.innerHTML = '';
    displayMessage('System', `Loaded session ${sessionId}`, 'system');
}