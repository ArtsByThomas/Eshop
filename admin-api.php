<?php
session_start();
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, PUT, DELETE');
header('Access-Control-Allow-Headers: Content-Type');

if (!isset($_SESSION['is_admin']) || $_SESSION['is_admin'] !== true) {
    http_response_code(403);
    echo json_encode(['error' => 'Neautorizovaný přístup. Prosím přihlaste se.']);
    exit;
}

require_once 'db.php';

$method = $_SERVER['REQUEST_METHOD'];
$action = $_GET['action'] ?? '';


if ($action === 'orders') {
    $ordersCollection = $db->orders;

    if ($method === 'GET') {
        $orders = $ordersCollection->find([], ['sort' => ['created_at' => -1]])->toArray();
        echo json_encode($orders);
        exit;
    }

    if ($method === 'POST') {
        $data = json_decode(file_get_contents('php://input'), true);
        $orderId = $data['id'] ?? null;
        $newStatus = $data['status'] ?? null;

        if ($orderId && $newStatus) {
            $ordersCollection->updateOne(
                ['_id' => new MongoDB\BSON\ObjectId($orderId)],
                ['$set' => ['status' => $newStatus]]
            );
            echo json_encode(['success' => true]);
        } else {
            echo json_encode(['success' => false, 'message' => 'Chybějící údaje.']);
        }
        exit;
    }
}

// ==========================================
// 2. PRODUKTY (PRODUCTS)
// ==========================================
if ($action === 'products') {
    $productsCollection = $db->products;

    if ($method === 'GET') {
        $products = $productsCollection->find()->toArray();
        echo json_encode($products);
        exit;
    }

    // NOVÉ: Vytvoření nového prázdného produktu
    if ($method === 'PUT') {
        $newProduct = [
            'name' => 'Nový produkt',
            'category' => 'Nezařazeno',
            'price' => '0 Kč',
            'image' => 'https://placehold.co/600x400/eeeeee/111111?text=Novy+produkt',
            'colors' => []
        ];
        $result = $productsCollection->insertOne($newProduct);
        echo json_encode(['success' => true, 'id' => (string)$result->getInsertedId()]);
        exit;
    }

    if ($method === 'POST') {
        $data = json_decode(file_get_contents('php://input'), true);
        $id = $data['id'] ?? null;
        $field = $data['field'] ?? null;
        $value = $data['value'] ?? null;

        if ($id && $field) {
            $productsCollection->updateOne(
                ['_id' => new MongoDB\BSON\ObjectId($id)],
                ['$set' => [$field => $value]]
            );
            echo json_encode(['success' => true]);
        } else {
            echo json_encode(['success' => false, 'message' => 'Nekompletní data']);
        }
        exit;
    }

    if ($method === 'DELETE') {
        $id = $_GET['id'] ?? null;
        if ($id) {
            $productsCollection->deleteOne(['_id' => new MongoDB\BSON\ObjectId($id)]);
            echo json_encode(['success' => true]);
        } else {
            echo json_encode(['success' => false]);
        }
        exit;
    }
}
?>