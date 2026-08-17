<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

$products = [
    [ 
        "id" => 1, 
        "name" => "Stylové tričko", 
        "price" => "499 Kč", 
        "image" => "https://placehold.co/600x400/eeeeee/111111?text=Bílé+tričko", 
        "category" => "Oblečení", 
        "subcategory" => "Trička",
        "description" => "Minimalistické bavlněné tričko s krátkým rukávem. Skvěle padne a je ideální na každodenní nošení.",
        "colors" => [
            [ 
                "name" => "Bílá", 
                "hex" => "#ffffff", 
                "images" => [
                    "https://placehold.co/600x400/eeeeee/111111?text=Bílé+tričko",
                    "https://placehold.co/600x400/eeeeee/111111?text=Bílé+tričko+(zezadu)",
                    "https://placehold.co/600x400/eeeeee/111111?text=Bílé+tričko+(detail)"
                ] 
            ],
            [ 
                "name" => "Černá", 
                "hex" => "#111111", 
                "images" => [
                    "https://placehold.co/600x400/111111/ffffff?text=Černé+tričko",
                    "https://placehold.co/600x400/111111/ffffff?text=Černé+tričko+(zezadu)"
                ] 
            ]
        ]
    ],
    [ 
        "id" => 2, 
        "name" => "Moderní mikina", 
        "price" => "899 Kč", 
        "image" => "https://placehold.co/600x400/333333/ffffff?text=Tmavá+mikina", 
        "category" => "Oblečení", 
        "subcategory" => "Mikiny",
        "description" => "Hřejivá mikina s kapucí z prémiové bavlny. Vybavena prostornou klokaní kapsou.",
        "colors" => [
            [ 
                "name" => "Tmavě šedá", 
                "hex" => "#333333", 
                "images" => [
                    "https://placehold.co/600x400/333333/ffffff?text=Tmavá+mikina"
                ] 
            ],
            [ 
                "name" => "Béžová", 
                "hex" => "#d4c5b9", 
                "images" => [
                    "https://placehold.co/600x400/d4c5b9/111111?text=Béžová+mikina",
                    "https://placehold.co/600x400/d4c5b9/111111?text=Béžová+mikina+(bez+kapuce)"
                ] 
            ]
        ]
    ],
    [ 
        "id" => 3, 
        "name" => "Černé džíny", 
        "price" => "1299 Kč", 
        "image" => "https://placehold.co/600x400/111111/ffffff?text=Černé+džíny", 
        "category" => "Oblečení", 
        "subcategory" => "Kalhoty",
        "description" => "Slim-fit džíny z mírně elastického denimu. Nabízí maximální komfort a moderní siluetu.",
        "colors" => []
    ]
];

$query = isset($_GET['q']) ? mb_strtolower(trim($_GET['q']), 'UTF-8') : null;
$category = isset($_GET['category']) ? $_GET['category'] : null;
$subcategory = isset($_GET['subcategory']) ? $_GET['subcategory'] : null;

$filteredProducts = array_filter($products, function($product) use ($query, $category, $subcategory) {
    $match = true;

    // 1. Filtr podle textu (vyhledávací pole)
    if ($query !== null && $query !== '') {
        $productName = mb_strtolower($product['name'], 'UTF-8');
        if (mb_strpos($productName, $query) === false) {
            $match = false;
        }
    }

    // 2. Filtr podle hlavní kategorie
    if ($category !== null && $category !== '') {
        if ($product['category'] !== $category) {
            $match = false;
        }
    }

    // 3. Filtr podle podkategorie
    if ($subcategory !== null && $subcategory !== '') {
        if ($product['subcategory'] !== $subcategory) {
            $match = false;
        }
    }

    return $match;
});

echo json_encode(array_values($filteredProducts));
?>