<?php
header('Content-Type: application/json');
require_once 'db.php'; 

$postData = json_decode(file_get_contents('php://input'), true);

if (!$postData || empty($postData['cart_items'])) {
    echo json_encode(["success" => false, "message" => "Objednávka je prázdná nebo chybí data."]);
    exit;
}

$ordersCollection = $db->orders; 

$orderDocument = [
    'user_id' => $postData['user_id'] ?? 'guest',
    'name' => htmlspecialchars($postData['name'] ?? ''),
    'street' => htmlspecialchars($postData['street'] ?? ''),
    'city' => htmlspecialchars($postData['city'] ?? ''),
    'zip' => htmlspecialchars($postData['zip'] ?? ''),
    'phone' => htmlspecialchars($postData['phone'] ?? ''),
    'email' => htmlspecialchars($postData['email'] ?? ''),
    'cart_items' => $postData['cart_items'], 
    'total' => $postData['total'] ?? 0, 
    'created_at' => date('d. m. Y v H:i'), 
    'timestamp' => new MongoDB\BSON\UTCDateTime() 
];

try {
    $ordersCollection->insertOne($orderDocument);
    echo json_encode(["success" => true, "message" => "Objednávka byla úspěšně uložena do databáze!"]);
} catch (Exception $e) {
    echo json_encode(["success" => false, "message" => "Chyba databáze: " . $e->getMessage()]);
}
?>