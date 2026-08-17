<?php
header('Content-Type: application/json');
require_once 'db.php'; // Načteme připojení k MongoDB

// Získáme data odeslaná z checkout.js
$postData = json_decode(file_get_contents('php://input'), true);

if (!$postData || empty($postData['cart_items'])) {
    echo json_encode(["success" => false, "message" => "Objednávka je prázdná nebo chybí data."]);
    exit;
}

$ordersCollection = $db->orders; // Připojíme se k tabulce (kolekci) 'orders'

// Připravíme dokument pro uložení do databáze
$orderDocument = [
    'user_id' => $postData['user_id'] ?? 'guest',
    'name' => htmlspecialchars($postData['name'] ?? ''),
    'street' => htmlspecialchars($postData['street'] ?? ''),
    'city' => htmlspecialchars($postData['city'] ?? ''),
    'zip' => htmlspecialchars($postData['zip'] ?? ''),
    'phone' => htmlspecialchars($postData['phone'] ?? ''),
    'email' => htmlspecialchars($postData['email'] ?? ''),
    'cart_items' => $postData['cart_items'], // Uložíme všechny položky v košíku
    'total' => $postData['total'] ?? 0, // Celková cena
    'created_at' => date('d. m. Y v H:i'), // Hezký formát data pro zobrazení
    'timestamp' => new MongoDB\BSON\UTCDateTime() // Strojový čas pro správné řazení
];

try {
    // Uložení do MongoDB
    $ordersCollection->insertOne($orderDocument);
    echo json_encode(["success" => true, "message" => "Objednávka byla úspěšně uložena do databáze!"]);
} catch (Exception $e) {
    echo json_encode(["success" => false, "message" => "Chyba databáze: " . $e->getMessage()]);
}
?>