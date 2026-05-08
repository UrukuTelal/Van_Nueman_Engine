#!/usr/bin/env python3
"""
Terminal Viewer Generator for OpenClaw CrowNest Dev Team
Generates an HTML terminal view with Pillar UID, color coding, and login.
Reads from http://localhost:8889/api/log
"""

import sys
import json
import time
from datetime import datetime
from pathlib import Path

def generate_terminal_html():
    """Generate the HTML terminal view with dark theme and color coding."""
    
    html_content = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device, initial-scale=1.0">
    <title>OpenClaw CrowNest Terminal</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            background-color: #1a1a1a;
            color: #ffffff;
            font-family: 'Courier New', 'Consolas', monospace;
            font-size: 14px;
            height: 100vh;
            display: flex;
            flex-direction: column;
        }
        
        #terminal-header {
            background-color: #2d2d2d;
            padding: 10px 20px;
            border-bottom: 2px solid #444;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        #terminal-header h1 {
            font-size: 16px;
            color: #00ffff;
        }
        
        #pillar-uid {
            font-size: 12px;
            color: #aaa;
            background: #333;
            padding: 5px 10px;
            border-radius: 4px;
        }
        
        #terminal-output {
            flex: 1;
            overflow-y: auto;
            padding: 10px;
            background-color: #1a1a1a;
        }
        
        .log-entry {
            margin-bottom: 5px;
            line-height: 1.4;
            white-space: pre-wrap;
            word-wrap: break-word;
        }
        
        .timestamp {
            color: #666;
            margin-right: 8px;
        }
        
        .user-sys {
            color: #00ffff;
        }
        
        .user-admin {
            color: #bf00ff;
            font-weight: bold;
        }
        
        .user-user {
            color: #ffffff;
        }
        
        .content {
            color: inherit;
        }
        
        .content.has-code {
            background: #252525;
            padding: 10px;
            border-left: 3px solid #555;
            margin: 5px 0;
            font-family: 'Consolas', monospace;
            font-size: 13px;
        }
        
        #terminal-input-area {
            background-color: #2d2d2d;
            padding: 10px;
            border-top: 2px solid #444;
            display: flex;
            gap: 10px;
        }
        
        #command-input {
            flex: 1;
            background: #1a1a1a;
            border: 1px solid #555;
            color: #fff;
            padding: 8px 12px;
            font-family: 'Courier New', monospace;
            font-size: 14px;
            border-radius: 4px;
        }
        
        #command-input:focus {
            outline: none;
            border-color: #00ffff;
        }
        
        #send-btn {
            background: #00ffff;
            color: #000;
            border: none;
            padding: 8px 20px;
            cursor: pointer;
            font-weight: bold;
            border-radius: 4px;
        }
        
        #send-btn:hover {
            background: #00cccc;
        }
        
        .login-hint {
            font-size: 11px;
            color: #888;
            margin-top: 5px;
        }
        
        .pillar-state {
            font-size: 11px;
            color: #ffa500;
            margin-left: 10px;
        }
        
        pre {
            margin: 0;
            white-space: pre-wrap;
        }
    </style>
</head>
<body>
    <div id="terminal-header">
        <h1>🔷 OpenClaw CrowNest Dev Team Terminal</h1>
        <div id="pillar-uid">
            <div>Pillar UID: <span id="pillar-state">Loading...</span></div>
            <div>Current User: <span id="current-user">SYS</span> | Color: <span id="user-color">cyan</span></div>
        </div>
    </div>
    
    <div id="terminal-output"></div>
    
    <div id="terminal-input-area">
        <input type="text" id="command-input" placeholder="Type command... (LOGIN ADMIN PapaMoshi or LOGIN SYS)" autofocus />
        <button id="send-btn">Send</button>
    </div>
    
    <div class="login-hint">
        💡 Login: Type "LOGIN ADMIN PapaMoshi" (purple) or "LOGIN SYS" (cyan) or "LOGIN USER" (white)
    </div>

    <script>
        const terminalOutput = document.getElementById('terminal-output');
        const commandInput = document.getElementById('command-input');
        const sendBtn = document.getElementById('send-btn');
        const pillarState = document.getElementById('pillar-state');
        const currentUser = document.getElementById('current-user');
        const userColor = document.getElementById('user-color');
        
        let lastMessageId = 0;
        let currentUsername = 'SYS';
        let pollInterval;
        
        function getColorClass(user) {
            if (user === 'ADMIN') return 'user-admin';
            if (user === 'SYS') return 'user-sys';
            return 'user-user';
        }
        
        function formatTimestamp(timestamp) {
            if (!timestamp) return '';
            // Extract time from ISO timestamp
            const parts = timestamp.split('T');
            if (parts.length > 1) {
                return parts[1].split('.')[0];  // HH:MM:SS
            }
            return timestamp;
        }
        
        function appendLogEntry(msg) {
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            
            const time = formatTimestamp(msg.timestamp);
            const userClass = getColorClass(msg.user || 'USER');
            const colorClass = getColorClass(msg.user || 'USER');
            
            let contentHtml = msg.content;
            if (msg.has_code) {
                contentHtml = '<pre>' + escapeHtml(msg.content) + '</pre>';
            } else {
                contentHtml = escapeHtml(msg.content);
            }
            
            // Add Pillar UID if present
            let pillarHtml = '';
            if (msg.pillar_uid) {
                pillarHtml = '<span class="pillar-state"> ' + msg.pillar_uid + '</span>';
            }
            
            entry.innerHTML = 
                '<span class="timestamp">' + time + '</span>' +
                '<span class="' + colorClass + '">' + (msg.user || 'USER') + '</span>: ' +
                '<span class="content' + (msg.has_code ? ' has-code' : '') + '">' + contentHtml + '</span>' +
                pillarHtml;
            
            terminalOutput.appendChild(entry);
            terminalOutput.scrollTop = terminalOutput.scrollHeight;
        }
        
        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
        
        async function pollLogs() {
            try {
                const response = await fetch('/api/log?since=' + lastMessageId);
                const data = await response.json();
                
                if (data.messages && data.messages.length > 0) {
                    data.messages.forEach(msg => {
                        appendLogEntry(msg);
                        lastMessageId = msg.id + 1;
                    });
                }
                
                // Update Pillar State
                if (data.pillar_uid) {
                    pillarState.textContent = JSON.stringify(data.pillar_uid).substring(0, 100) + '...';
                }
                
                // Update current user
                if (data.current_user) {
                    currentUsername = data.current_user;
                    currentUser.textContent = currentUsername;
                    userColor.textContent = data.user_color || 'white';
                    userColor.style.color = data.user_color || '#ffffff';
                }
                
            } catch (error) {
                console.error('Poll error:', error);
            }
        }
        
        async function sendCommand() {
            const command = commandInput.value.trim();
            if (!command) return;
            
            try {
                const response = await fetch('/api/input', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({input: command})
                });
                
                commandInput.value = '';
                
                // If login command, update UI immediately
                if (command.toUpperCase().startsWith('LOGIN ')) {
                    const parts = command.split(' ');
                    if (parts.length >= 2) {
                        const role = parts[1].toUpperCase();
                        if (role === 'ADMIN') {
                            currentUsername = 'ADMIN';
                            currentUser.textContent = 'ADMIN';
                            userColor.textContent = 'purple';
                            userColor.style.color = '#bf00ff';
                        } else if (role === 'SYS') {
                            currentUsername = 'SYS';
                            currentUser.textContent = 'SYS';
                            userColor.textContent = 'cyan';
                            userColor.style.color = '#00ffff';
                        } else if (role === 'USER') {
                            currentUsername = 'USER';
                            currentUser.textContent = 'USER';
                            userColor.textContent = 'white';
                            userColor.style.color = '#ffffff';
                        }
                    }
                }
                
            } catch (error) {
                console.error('Send error:', error);
            }
        }
        
        sendBtn.addEventListener('click', sendCommand);
        commandInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') sendCommand();
        });
        
        // Start polling
        pollInterval = setInterval(pollLogs, 1000);
        pollLogs();  // Initial poll
        
        // Focus input on load
        commandInput.focus();
    </script>
</body>
</html>
"""
    return html_content

def save_terminal_html():
    """Save the generated HTML to the scripts directory."""
    output_path = Path(__file__).parent / "terminal_view.html"
    
    html = generate_terminal_html()
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html)
    
    print(f"[Terminal Viewer] Generated: {output_path}")
    print(f"[Terminal Viewer] Open in browser: http://localhost:8889/terminal_view.html")
    return output_path

if __name__ == "__main__":
    print("OpenClaw CrowNest Terminal Viewer Generator")
    print("=" * 50)
    
    save_terminal_html()
    
    print("\nFeatures:")
    print("  ✅ Dark theme terminal with color coding")
    print("  ✅ ADMIN = Purple (#bf00ff)")
    print("  ✅ SYS = Cyan (#00ffff)")
    print("  ✅ USER = White (#ffffff)")
    print("  ✅ Pillar UID (State Vector) display")
    print("  ✅ Timestamps (HH:MM:SS format)")
    print("  ✅ Login prompt: LOGIN ADMIN PapaMoshi / LOGIN SYS")
    print("  ✅ Auto-polling every 1 second")
    print("  ✅ Code block detection and formatting")
    print("=" * 50)
