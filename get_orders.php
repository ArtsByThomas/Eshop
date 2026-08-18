<?php
header('Content-Type: application/json');
require_once 'db.php';

$userId = $_GET['user_id'] ?? null;

if (!$userId) {
    echo json_encode([]); 
    exit;
}

$ordersCollection = $db->orders;

try {
    $cursor = $ordersCollection->find(
        ['user_id' => $userId],
        ['sort' => ['timestamp' => 1]]
    );

    $orders = [];
    foreach ($cursor as $doc) {
        $orders[] = [
            'id' => (string)$doc['_id'],
            'created_at' => $doc['created_at'] ?? 'Neznámé datum',
            'total' => $doc['total'] ?? 0,
            'status' => $doc['status'] ?? 'Nová - Zpracovává se',
            'street' => $doc['street'] ?? '',
            'city' => $doc['city'] ?? '',
            'zip' => $doc['zip'] ?? '',
            'cart_items' => $doc['cart_items'] ?? []
        ];
    }

    echo json_encode($orders);

} catch (Exception $e) {
    echo json_encode(["error" => "Chyba při komunikaci s databází: " . $e->getMessage()]);
}
?>