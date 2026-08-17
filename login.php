<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST');

$json_data = file_get_contents('php://input');
$data = json_decode($json_data, true);

if (isset($data['username']) && isset($data['password'])) {
    echo json_encode([
        "success" => true,
        "message" => "Úspěšně přihlášeno!"
    ]);
} else {
    http_response_code(400);
    echo json_encode([
        "success" => false,
        "message" => "Chybí jméno nebo heslo."
    ]);
}
?>