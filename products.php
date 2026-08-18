<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

// Připojíme databázi
require_once 'db.php'; 

$query = isset($_GET['q']) ? trim($_GET['q']) : null;
$category = isset($_GET['category']) ? $_GET['category'] : null;
$subcategory = isset($_GET['subcategory']) ? $_GET['subcategory'] : null;

// Sestavení vyhledávacího filtru pro MongoDB
$filter = [];

// 1. Filtr podle textu (vyhledávací pole - hledá case-insensitive)
if ($query !== null && $query !== '') {
    $filter['name'] = new MongoDB\BSON\Regex($query, 'i'); 
}

// 2. Filtr podle hlavní kategorie
if ($category !== null && $category !== '') {
    $filter['category'] = $category;
}

// 3. Filtr podle podkategorie
if ($subcategory !== null && $subcategory !== '') {
    $filter['subcategory'] = $subcategory;
}

// Načtení z MongoDB
$productsCollection = $db->products;
$cursor = $productsCollection->find($filter);

$filteredProducts = [];

foreach ($cursor as $doc) {
    $product = (array) $doc;
    
    $product['id'] = (string) $doc['_id']; 
    
    unset($product['_id']);
    
    $filteredProducts[] = $product;
}

echo json_encode($filteredProducts);
?>