<?php
require_once __DIR__ . '/vendor/autoload.php'; 

try {
    $client = new MongoDB\Client("mongodb://localhost:27017");
    $db = $client->eshop; 
} catch (Exception $e) {
    header('Content-Type: application/json');
    echo json_encode(["success" => false, "message" => "Chyba připojení k databázi: " . $e->getMessage()]);
    exit;
}
?>