<?php
header('Content-Type: application/json');
require_once 'db.php';

$data = json_decode(file_get_contents('php://input'), true);
$action = isset($_GET['action']) ? $_GET['action'] : '';
$usersCollection = $db->users; // Kolekce "users" v MongoDB

// 1. REGISTRACE
if ($action === 'register') {
    $email = trim($data['email'] ?? '');
    $password = $data['password'] ?? '';
    $name = trim($data['name'] ?? '');

    // Zkontrolujeme, zda už e-mail v databázi není
    $existingUser = $usersCollection->findOne(['email' => $email]);
    if ($existingUser) {
        echo json_encode(["success" => false, "message" => "Uživatel s tímto e-mailem již existuje."]);
        exit;
    }

    // Bezpečné uložení hesla (hash) a zápis do MongoDB
    $usersCollection->insertOne([
        'email' => $email,
        'name' => $name,
        'password' => password_hash($password, PASSWORD_DEFAULT),
        'created_at' => new MongoDB\BSON\UTCDateTime()
    ]);
    echo json_encode(["success" => true, "message" => "Účet byl vytvořen. Můžete se přihlásit."]);
    exit;
}

// 2. PŘIHLÁŠENÍ
if ($action === 'login') {
    $email = trim($data['email'] ?? '');
    $password = $data['password'] ?? '';

    $user = $usersCollection->findOne(['email' => $email]);
    
    // Ověření hashe hesla
    if ($user && password_verify($password, $user['password'])) {
        echo json_encode(["success" => true, "message" => "Přihlášení úspěšné.", "user_id" => $user['email']]);
    } else {
        echo json_encode(["success" => false, "message" => "Nesprávný e-mail nebo heslo."]);
    }
    exit;
}

// 3. ZAPOMENUTÉ HESLO
if ($action === 'reset_request') {
    $email = trim($data['email'] ?? '');
    
    $user = $usersCollection->findOne(['email' => $email]);
    if ($user) {
        // Zde by normálně proběhlo generování tokenu a odeslání reálného e-mailu (např. přes PHPMailer)
        // Prozatím simulujeme úspěch
        echo json_encode(["success" => true, "message" => "Pokud e-mail existuje v naší databázi, odeslali jsme na něj instrukce k obnově hesla."]);
    } else {
        // Z bezpečnostních důvodů (ochrana proti zjišťování emailů) vracíme stejnou hlášku
        echo json_encode(["success" => true, "message" => "Pokud e-mail existuje v naší databázi, odeslali jsme na něj instrukce k obnově hesla."]);
    }
    exit;
}
?>