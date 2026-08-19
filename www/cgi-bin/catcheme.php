#!/usr/bin/env php
<?php
// Must print headers first for CGI to work perfectly!
// echo "Content-Type: text/html\r\n\r\n";

// Use PHP Heredoc to output the HTML seamlessly
echo <<<HTML
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Evaluator Decision</title>
    <style>
        body {
            background-color: #0d1117;
            color: #c9d1d9;
            font-family: 'Courier New', Courier, monospace;
            text-align: center;
            height: 100vh;
            margin: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            overflow: hidden;
        }
        h1 {
            color: #58a6ff;
            text-shadow: 0 0 15px rgba(88, 166, 255, 0.4);
            margin-bottom: 60px;
            font-size: 2.5rem;
        }
        .buttons-container {
            display: flex;
            gap: 150px;
            position: relative;
            width: 100%;
            justify-content: center;
            height: 100px;
        }
        button {
            padding: 20px 50px;
            font-size: 1.5rem;
            font-weight: bold;
            border: none;
            border-radius: 50px;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
        }
        .btn-good {
            background: linear-gradient(135deg, #00e5ff 0%, #39d353 100%);
            color: #000;
            z-index: 10;
        }
        .btn-good:hover {
            transform: scale(1.1);
            box-shadow: 0 4px 30px rgba(57, 211, 83, 0.6);
        }
        .btn-bad {
            background: linear-gradient(135deg, #ff0055 0%, #ff00aa 100%);
            color: #fff;
            position: absolute;
            left: 60%;
            /* Removed transition so it teleports instantly! */
        }
        
        #success-msg {
            display: none;
            font-size: 3rem;
            color: #39d353;
            animation: zoomIn 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            text-shadow: 0 0 30px rgba(57, 211, 83, 0.5);
        }
        
        .footer {
            position: fixed;
            bottom: 20px;
            color: #8b949e;
        }

        @keyframes zoomIn {
            from { transform: scale(0); opacity: 0; }
            to { transform: scale(1); opacity: 1; }
        }
    </style>
</head>
<body>
    <h1 id="title">Hey Evaluator! What score do we deserve?</h1>
    
    <div class="buttons-container" id="btn-container">
        <button class="btn-good" onclick="success()">Give 125/100 🏆</button>
        <button class="btn-bad" id="badBtn">Give 0/100 😭</button>
    </div>

    <div id="success-msg">
        🎉 PERFECT CHOICE! 🎉<br>
        <p style="font-size: 1.5rem; color: #c9d1d9; margin-top: 20px;">You are the best evaluator ever!</p>
    </div>

    <div class="footer">
        Generated dynamically via PHP CGI
    </div>

    <script>
        const badBtn = document.getElementById('badBtn');
        
        badBtn.addEventListener('mouseover', function() {
            escapeButton();
        });
        
        badBtn.addEventListener('touchstart', function(e) {
            e.preventDefault();
            escapeButton();
        });
        
        // Just in case they are crazy fast or use Keyboard TAB + Enter
        badBtn.addEventListener('click', function(e) {
            e.preventDefault();
            escapeButton();
        });


        function escapeButton() {
            const maxX = window.innerWidth - badBtn.offsetWidth - 50;
            const maxY = window.innerHeight - badBtn.offsetHeight - 50;
            
            const randomX = Math.max(50, Math.floor(Math.random() * maxX));
            const randomY = Math.max(50, Math.floor(Math.random() * maxY));
            
            badBtn.style.position = 'fixed';
            badBtn.style.left = randomX + 'px';
            badBtn.style.top = randomY + 'px';
        }

        function success() {
            document.getElementById('btn-container').style.display = 'none';
            document.getElementById('title').style.display = 'none';
            document.getElementById('success-msg').style.display = 'block';
        }
    </script>
</body>
</html>
HTML;
?>