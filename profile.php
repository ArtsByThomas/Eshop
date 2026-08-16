<?php
header('Content-Type: application/json');

$jsonFile = 'users.json';

$user_id = "1"; 

$method = $_SERVER['REQUEST_METHOD'];

if (!file_exists($jsonFile)) {
    file_put_contents($jsonFile, json_encode([$user_id => []]));
}

$currentData = json_decode(file_get_contents($jsonFile), true);


if ($method === 'GET') {
    if (isset($currentData[$user_id])) {
        echo json_encode($currentData[$user_id]);
    } else {
        echo json_encode(["error" => "Uživatel nenalezen"]);
    }
    exit;
}


if ($method === 'POST') {
    $postData = json_decode(file_get_contents('php://input'), true);

    $currentData[$user_id]['name']   = htmlspecialchars($postData['name'] ?? '');
    $currentData[$user_id]['street'] = htmlspecialchars($postData['street'] ?? '');
    $currentData[$user_id]['zip']    = htmlspecialchars($postData['zip'] ?? '');
    $currentData[$user_id]['city']   = htmlspecialchars($postData['city'] ?? '');
    $currentData[$user_id]['phone']  = htmlspecialchars($postData['phone'] ?? '');
    $currentData[$user_id]['email']  = htmlspecialchars($postData['email'] ?? '');


    if (file_put_contents($jsonFile, json_encode($currentData, JSON_PRETTY_PRINT))) {
        echo json_encode(["success" => true, "message" => "Údaje byly úspěšně uloženy do souboru!"]);
    } else {
        http_response_code(500);
        echo json_encode(["success" => false, "message" => "Chyba při zápisu do souboru."]);
    }
    exit;
}
?>