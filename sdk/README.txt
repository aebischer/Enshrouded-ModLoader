SDK Enshrouded ModLoader — ABI 1

Référence : STANDARD_V1.txt.
Tutoriel de bout en bout : CREER_UN_MOD.md (dossiers, formats, compilation,
installation, ressources, diagnostic et liste de vérification de publication).
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

Localisation facultative C++17 : include/enshrouded_i18n.h. Voir la section
LOCALISATION du standard ; les lignes text nécessitent le chargeur 0.6.10.

GUIDE MULTILINGUE : LOCALISATION.txt contient un manifeste complet, les règles
de repli et la marche à suivre pour les textes d'objets. Le modèle example
est maintenant bilingue français/anglais (chargeur 0.6.10 minimum).
Catalogue C++17 testé : example/localized_texts.h.
