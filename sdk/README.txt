SDK Enshrouded ModLoader — ABI 1

Référence : STANDARD_V1.txt.
Header public unique : include/enshrouded_mod.h (C et C++ Windows x64).
Mod minimal : example/example_mod.cpp + example_mod.dll.mod.

Depuis la racine du projet : build-sdk.cmd compile le modèle, le validateur et
le test d'intégration. Les sorties restent sous build ; aucun fichier n'est
installé dans le jeu et la DLL du chargeur n'est pas remplacée.
Prérequis du projet : objets MinHook produits par build.cmd.

Le modèle seul est compilable depuis une invite x64 Native Tools de Visual Studio :
  example\build.cmd C:\chemin\vers\paquet
Le paquet à distribuer est le sous-dossier Mods de ce résultat. Les objets et la
bibliothèque d'importation sont des fichiers de compilation, pas des mods à livrer.

Validation depuis la racine du projet :
  build\validate_mod_package.exe build\sdk-package example/example_mod.dll

Le SDK n'offre pas encore de fonctions de manipulation des coffres ni de réseau.
Il standardise l'intégration au chargeur, pas les mécaniques natives du jeu.
