<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

$products = [
    [
        "id" => 1,
        "name" => "Stylové tričko",
        "price" => "499 Kč",
        "image" => "https://placehold.co/600x400/png"
    ],
    [
        "id" => 2,
        "name" => "Moderní mikina",
        "price" => "899 Kč",
        "image" => "https://placehold.co/600x400/png"
    ],
    [
        "id" => 3,
        "name" => "Černé džíny",
        "price" => "1299 Kč",
        "image" => "https://placehold.co/600x400/png"
    ],
    [
        "id" => 4,
        "name" => "Zimní čepice",
        "price" => "299 Kč",
        "image" => "https://placehold.co/600x400/png"
    ]
];

echo json_encode($products);
?>