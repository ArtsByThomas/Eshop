<?php
session_start();
header('Content-Type: application/json');

$method = $_SERVER['REQUEST_METHOD'];
$action = $_GET['action'] ?? '';

if ($method === 'POST' && $action === 'login') {
    $data = json_decode(file_get_contents('php://input'), true);
    $username = $data['username'] ?? '';
    $password = $data['password'] ?? '';

    $adminUser = 'admin';
    $adminPass = 'tajneheslo123';

    if ($username === $adminUser && $password === $adminPass) {
        $_SESSION['is_admin'] = true; 
        echo json_encode(['success' => true]);
    } else {
        echo json_encode(['success' => false, 'message' => 'Nesprávné přihlašovací údaje.']);
    }
    exit;
}

if ($method === 'GET' && $action === 'check') {
    if (isset($_SESSION['is_admin']) && $_SESSION['is_admin'] === true) {
        echo json_encode(['logged_in' => true]);
    } else {
        echo json_encode(['logged_in' => false]);
    }
    exit;
}

if ($method === 'GET' && $action === 'logout') {
    session_destroy();
    echo json_encode(['success' => true]);
    exit;
}
?>