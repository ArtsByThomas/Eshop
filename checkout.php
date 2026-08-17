<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST');

$json_data = file_get_contents('php://input');
$data = json_decode($json_data, true);

if ($data) {
    echo json_encode([
        "success" => true,
        "message" => "Údaje byly úspěšně uloženy!"
    ]);
} else {
    http_response_code(400);
    echo json_encode([
        "success" => false,
        "message" => "Neplatná data."
    ]);
}
?>