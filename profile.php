<?php
header('Content-Type: application/json');
require_once 'db.php';

$method = $_SERVER['REQUEST_METHOD'];
$usersCollection = $db->users;

// NAČTENÍ DAT PROFILU
if ($method === 'GET') {
    $email = $_GET['user_id'] ?? null;
    if (!$email) {
        echo json_encode(["error" => "Chybí identifikátor."]); 
        exit;
    }
    
    $user = $usersCollection->findOne(['email' => $email]);
    if ($user) {
        unset($user['password']); // Z bezpečnostních důvodů nikdy neposíláme heslo na frontend
        echo json_encode($user);
    } else {
        echo json_encode(["error" => "Uživatel nenalezen"]);
    }
    exit;
}

// ULOŽENÍ ZMĚN NEBO HESLA
if ($method === 'POST') {
    $postData = json_decode(file_get_contents('php://input'), true);
    $email = $postData['user_id'] ?? null;
    $action = $_GET['action'] ?? 'update_profile';

    if (!$email) {
        echo json_encode(["success" => false, "message" => "Chybí identifikátor uživatele."]); 
        exit;
    }

    // A) ZMĚNA HESLA
    if ($action === 'change_password') {
        $oldPassword = $postData['old_password'] ?? '';
        $newPassword = $postData['new_password'] ?? '';

        $user = $usersCollection->findOne(['email' => $email]);
        
        // Ověříme staré heslo
        if ($user && password_verify($oldPassword, $user['password'])) {
            $usersCollection->updateOne(
                ['email' => $email],
                ['$set' => ['password' => password_hash($newPassword, PASSWORD_DEFAULT)]]
            );
            echo json_encode(["success" => true, "message" => "Heslo bylo úspěšně změněno."]);
        } else {
            echo json_encode(["success" => false, "message" => "Současné heslo není správné."]);
        }
        exit;
    }

   $newEmail = trim($postData['email'] ?? '');

    // Pokud uživatel mění e-mail za jiný, zkontrolujeme, zda už neexistuje
    if ($newEmail !== $email) {
        $existingUser = $usersCollection->findOne(['email' => $newEmail]);
        if ($existingUser) {
            echo json_encode(["success" => false, "message" => "Tento e-mail již používá jiný účet."]);
            exit;
        }
    }

    $updateData = [
        'name' => htmlspecialchars($postData['name'] ?? ''),
        'street' => htmlspecialchars($postData['street'] ?? ''),
        'zip' => htmlspecialchars($postData['zip'] ?? ''),
        'city' => htmlspecialchars($postData['city'] ?? ''),
        'phone' => htmlspecialchars($postData['phone'] ?? ''),
        'email' => htmlspecialchars($newEmail) // Uložíme nový (nebo původní) e-mail
    ];

    $usersCollection->updateOne(
        ['email' => $email], // Hledáme uživatele podle původního e-mailu
        ['$set' => $updateData],
        ['upsert' => true]
    );

    // Vracíme i nový e-mail, aby si ho mohl frontend přepsat
    echo json_encode([
        "success" => true, 
        "message" => "Údaje byly uloženy do databáze!",
        "new_email" => $newEmail
    ]);
    exit;
}
?>