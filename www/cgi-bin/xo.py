#!/usr/bin/env python3
import sys

html_content = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>42 Tic-Tac-Toe vs Evaluator</title>
    <style>
        body {
            background-color: #0d1117;
            color: #c9d1d9;
            font-family: 'Courier New', Courier, monospace;
            text-align: center;
            margin: 0;
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
            justify-content: center;
        }
        h1 {
            color: #58a6ff;
            text-shadow: 0 0 10px rgba(88, 166, 255, 0.5);
            margin-bottom: 5px;
        }
        p.subtitle {
            color: #8b949e;
            margin-bottom: 30px;
        }
        
        .status {
            font-size: 1.8rem;
            margin-bottom: 10px;
            font-weight: bold;
            height: 40px;
        }
        .timer {
            font-size: 1.2rem;
            margin-bottom: 20px;
            color: #ff7b72;
            font-weight: bold;
            height: 20px;
        }
        .turn-x { color: #00e5ff; text-shadow: 0 0 10px rgba(0, 229, 255, 0.5); }
        .turn-o { color: #ff0055; text-shadow: 0 0 10px rgba(255, 0, 85, 0.5); }
        
        .board {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            background-color: #30363d;
            padding: 10px;
            border-radius: 15px;
            box-shadow: 0 0 30px rgba(0, 0, 0, 0.5);
        }
        
        .cell {
            width: 100px;
            height: 100px;
            background-color: #161b22;
            border-radius: 10px;
            display: flex;
            justify-content: center;
            align-items: center;
            font-size: 4rem;
            font-family: sans-serif;
            font-weight: bold;
            cursor: pointer;
            transition: background-color 0.2s, transform 0.1s;
        }
        
        .cell:hover {
            background-color: #21262d;
        }
        
        .cell:active {
            transform: scale(0.95);
        }
        
        .cell.x { color: #00e5ff; text-shadow: 0 0 15px rgba(0, 229, 255, 0.6); }
        .cell.o { color: #ff0055; text-shadow: 0 0 15px rgba(255, 0, 85, 0.6); }
        .cell.winning { background-color: #2ea043; color: #fff; text-shadow: none; }

        button {
            background: linear-gradient(135deg, #00e5ff 0%, #0077ff 100%);
            border: none;
            padding: 15px 40px;
            font-size: 1.2rem;
            font-weight: bold;
            color: #000;
            border-radius: 50px;
            cursor: pointer;
            box-shadow: 0 4px 20px rgba(0, 229, 255, 0.4);
            margin-top: 40px;
            transition: transform 0.2s;
            text-transform: uppercase;
        }
        
        button:hover {
            transform: scale(1.05);
        }
    </style>
</head>
<body>
    <h1>⚔️ WebServ Tic-Tac-Toe ⚔️</h1>
    <p class="subtitle">Student (X) vs Evaluator (O)</p>
    
    <div id="status" class="status turn-x">Student's Turn (X)</div>
    <div id="timer" class="timer">Time left: 5.0s</div>
    
    <div class="board" id="board">
        <div class="cell" data-index="0"></div>
        <div class="cell" data-index="1"></div>
        <div class="cell" data-index="2"></div>
        <div class="cell" data-index="3"></div>
        <div class="cell" data-index="4"></div>
        <div class="cell" data-index="5"></div>
        <div class="cell" data-index="6"></div>
        <div class="cell" data-index="7"></div>
        <div class="cell" data-index="8"></div>
    </div>
    
    <button onclick="resetGame()">Restart Match</button>

    <script>
        const cells = document.querySelectorAll('.cell');
        const statusText = document.getElementById('status');
        
        let boardState = ["", "", "", "", "", "", "", "", ""];
        let currentPlayer = "X";
        let gameActive = true;
        let turnTimer;
        let timeLeft = 5.0;
        
        function startTimer() {
            clearInterval(turnTimer);
            if (!gameActive) return;
            
            timeLeft = 5.0;
            document.getElementById('timer').innerText = `Time left: ${timeLeft.toFixed(1)}s`;
            
            turnTimer = setInterval(() => {
                if (!gameActive) {
                    clearInterval(turnTimer);
                    return;
                }
                
                timeLeft -= 0.1;
                if (timeLeft <= 0) {
                    clearInterval(turnTimer);
                    timeLeft = 0;
                    document.getElementById('timer').innerText = `Time left: 0.0s`;
                    handleTimeout();
                } else {
                    document.getElementById('timer').innerText = `Time left: ${timeLeft.toFixed(1)}s`;
                }
            }, 100);
        }
        
        function handleTimeout() {
            gameActive = false;
            statusText.innerText = `⏳ Time's up! ${currentPlayer === 'X' ? 'Evaluator (O)' : 'Student (X)'} Wins!`;
            statusText.className = "status";
            statusText.style.color = currentPlayer === 'X' ? "#ff7b72" : "#39d353";
        }
        
        
        const winningConditions = [
            [0, 1, 2], [3, 4, 5], [6, 7, 8], // Rows
            [0, 3, 6], [1, 4, 7], [2, 5, 8], // Columns
            [0, 4, 8], [2, 4, 6]             // Diagonals
        ];
        
        function handleCellClick(e) {
            const cell = e.target;
            const index = parseInt(cell.getAttribute('data-index'));
            
            if (boardState[index] !== "" || !gameActive) return;
            
            boardState[index] = currentPlayer;
            cell.innerText = currentPlayer;
            cell.classList.add(currentPlayer.toLowerCase());
            
            checkWin();
        }
        
        function checkWin() {
            let roundWon = false;
            let winningCells = [];
            
            for (let i = 0; i < winningConditions.length; i++) {
                const [a, b, c] = winningConditions[i];
                if (boardState[a] && boardState[a] === boardState[b] && boardState[a] === boardState[c]) {
                    roundWon = true;
                    winningCells = [a, b, c];
                    break;
                }
            }
            
            if (roundWon) {
                statusText.innerText = currentPlayer === 'X' 
                    ? "🎉 Student Wins! 125/100! 🎉" 
                    : "💀 Evaluator Wins! 0/100! 💀";
                statusText.className = "status";
                statusText.style.color = currentPlayer === 'X' ? "#39d353" : "#ff7b72";
                
                winningCells.forEach(i => cells[i].classList.add('winning'));
                gameActive = false;
                return;
            }
            
            if (!boardState.includes("")) {
                statusText.innerText = "🤝 Draw! Try Again! 🤝";
                statusText.className = "status";
                statusText.style.color = "#c9d1d9";
                gameActive = false;
                return;
            }
            
            currentPlayer = currentPlayer === "X" ? "O" : "X";
            if (currentPlayer === "X") {
                statusText.innerText = "Student's Turn (X)";
                statusText.className = "status turn-x";
            } else {
                statusText.innerText = "Evaluator's Turn (O)";
                statusText.className = "status turn-o";
            }
            startTimer();
        }
        
        function resetGame() {
            boardState = ["", "", "", "", "", "", "", "", ""];
            currentPlayer = "X";
            gameActive = true;
            statusText.innerText = "Student's Turn (X)";
            statusText.className = "status turn-x";
            statusText.style.color = "";
            
            cells.forEach(cell => {
                cell.innerText = "";
                cell.className = "cell";
            });
            startTimer();
        }
        
        cells.forEach(cell => cell.addEventListener('click', handleCellClick));
        startTimer(); // Start the first turn timer!
    </script>
</body>
</html>
"""

# Guarantee perfect \r\n line endings for the HTTP parser!
sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write(f"Content-Length: {len(html_content)}\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(html_content)
