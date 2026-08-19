#!/usr/bin/php-cgi
<?php

// Scientific Data Library (Simulated Database)
$planets = [
    "mercury" => [
        "name" => "Mercury",
        "emoji" => "🌑",
        "gravity" => 3.7, // m/s^2
        "type" => "Terrestrial Planet",
        "description" => "Mercury is the smallest planet in the Solar System and the closest to the Sun. Despite being closest to the Sun, it is not the hottest planet; that title belongs to Venus. It has no atmosphere to retain heat, leading to extreme temperature fluctuations."
    ],
    "venus" => [
        "name" => "Venus",
        "emoji" => "🌕",
        "gravity" => 8.87,
        "type" => "Terrestrial Planet",
        "description" => "Venus is the second planet from the Sun and our closest planetary neighbor. It is a world of intense heat and volcanic activity. Its thick atmosphere traps heat in a runaway greenhouse effect, making it the hottest planet in our solar system."
    ],
    "earth" => [
        "name" => "Earth",
        "emoji" => "🌍",
        "gravity" => 9.81,
        "type" => "Terrestrial Planet",
        "description" => "Our home planet is the third planet from the Sun, and the only place we know of so far that's inhabited by living things. It's the only world in our solar system with liquid water on the surface."
    ],
    "mars" => [
        "name" => "Mars",
        "emoji" => "🔴",
        "gravity" => 3.71,
        "type" => "Terrestrial Planet",
        "description" => "Mars is a dusty, cold, desert world with a very thin atmosphere. There is strong evidence Mars was—billions of years ago—wetter and warmer, with a thicker atmosphere."
    ],
    "jupiter" => [
        "name" => "Jupiter",
        "emoji" => "🪐",
        "gravity" => 24.79,
        "type" => "Gas Giant",
        "description" => "Jupiter is more than twice as massive than the other planets of our solar system combined. The giant planet's Great Red Spot is a centuries-old storm bigger than Earth."
    ]
];

// Handle GET request for navigation
$selected_planet = isset($_GET['planet']) && array_key_exists($_GET['planet'], $planets) ? $_GET['planet'] : 'earth';
$planet = $planets[$selected_planet];

// Handle POST request for Gravity Calculator
$earth_weight = 0;
$alien_weight = 0;
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['earth_weight'])) {
    $earth_weight = floatval($_POST['earth_weight']);
    // Formula: Weight on Planet = (Weight on Earth / Earth Gravity) * Planet Gravity
    $alien_weight = ($earth_weight / $planets['earth']['gravity']) * $planet['gravity'];
}
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Scientific Explorer | Webserv CGI</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --text-color: #f8fafc;
            --card-bg: #1e293b;
            --accent: #38bdf8;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-color);
            line-height: 1.6;
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        h1 { text-align: center; color: var(--accent); }
        .nav-bar {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin-bottom: 30px;
            flex-wrap: wrap;
        }
        .nav-btn {
            background-color: var(--card-bg);
            color: var(--text-color);
            text-decoration: none;
            padding: 10px 15px;
            border-radius: 5px;
            border: 1px solid #334155;
            transition: 0.3s;
        }
        .nav-btn:hover, .nav-btn.active {
            background-color: var(--accent);
            color: #000;
        }
        .content-card {
            background-color: var(--card-bg);
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
        }
        .planet-header {
            display: flex;
            align-items: center;
            gap: 20px;
            border-bottom: 1px solid #334155;
            padding-bottom: 20px;
            margin-bottom: 20px;
        }
        .emoji { font-size: 4rem; }
        .tag {
            background-color: #475569;
            padding: 5px 10px;
            border-radius: 20px;
            font-size: 0.8rem;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .calculator {
            margin-top: 30px;
            padding-top: 20px;
            border-top: 1px dashed #334155;
        }
        input[type="number"] {
            padding: 10px;
            border-radius: 5px;
            border: none;
            width: 150px;
        }
        button {
            padding: 10px 20px;
            background-color: var(--accent);
            color: #000;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-weight: bold;
        }
        .result {
            margin-top: 15px;
            font-size: 1.2rem;
            color: #4ade80;
        }
    </style>
</head>
<body>

<div class="container">
    <h1>🌌 Solar System Encyclopedia</h1>
    
    <!-- Navigation (Tests GET Requests) -->
    <div class="nav-bar">
        <?php foreach ($planets as $key => $p): ?>
            <a href="?planet=<?php echo $key; ?>" class="nav-btn <?php echo $selected_planet === $key ? 'active' : ''; ?>">
                <?php echo $p['emoji'] . " " . $p['name']; ?>
            </a>
        <?php endforeach; ?>
    </div>

    <!-- Content Area -->
    <div class="content-card">
        <div class="planet-header">
            <div class="emoji"><?php echo $planet['emoji']; ?></div>
            <div>
                <h2 style="margin: 0;"><?php echo $planet['name']; ?></h2>
                <span class="tag"><?php echo $planet['type']; ?></span>
            </div>
        </div>
        
        <h3>About <?php echo $planet['name']; ?></h3>
        <p><?php echo $planet['description']; ?></p>
        <p><strong>Surface Gravity:</strong> <?php echo $planet['gravity']; ?> m/s²</p>

        <!-- Form (Tests POST Requests) -->
        <div class="calculator">
            <h3>⚖️ Gravity Calculator</h3>
            <p>Calculate your weight on <?php echo $planet['name']; ?>:</p>
            <form method="POST" action="?planet=<?php echo $selected_planet; ?>">
                <input type="number" name="earth_weight" placeholder="Weight on Earth (kg)" value="<?php echo $earth_weight ?: ''; ?>" required min="1">
                <button type="submit">Calculate</button>
            </form>

            <?php if ($alien_weight > 0): ?>
                <div class="result">
                    If you weigh <?php echo $earth_weight; ?> kg on Earth, you would weigh <strong><?php echo number_format($alien_weight, 1); ?> kg</strong> on <?php echo $planet['name']; ?>!
                </div>
            <?php endif; ?>
        </div>
    </div>
</div>

</body>
</html>