# Enshrouded ModLoader 0.6.9

Chargeur natif Windows x64 pour Enshrouded, avec une interface MODS intégrée
au menu Modifier de chaque monde. Activation et paramètres sont enregistrés
séparément pour chaque monde.

Ce projet indépendant n'est pas un SDK officiel de Keen Games.
Le fonctionnement sur serveur dédié reste
à développer : voir les [décisions multijoueur](docs/server-multiplayer-decisions.txt).

## Compiler

Installer Visual Studio 2022 Build Tools avec les outils C++ x64 et le SDK Windows.
Les scripts actuels attendent l'installation dans
'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'.
MinHook est inclus sous sa [licence d'origine](third_party/minhook/LICENSE.txt).

Depuis la racine du dépôt, exécuter dans cet ordre :

1. `build.cmd` : produit `dist/dinput8.dll`.
2. `build-test-mods.cmd` : construit et teste les deux mods de démonstration.
3. `build-sdk.cmd` : construit l'exemple, le validateur et les tests du SDK.

Les sources et les scripts sont versionnés. Les binaires compilés, fichiers du
jeu, sauvegardes, journaux et configurations personnelles sont exclus.

## Installer et utiliser

Fermer le jeu, puis lancer `Installer.cmd`. Pour un autre dossier de jeu,
consulter les paramètres de `install.ps1`. L'installateur vérifie le format
DLL x64 et les exports du proxy, conserve une version précédente et restaure
l'état antérieur si la publication échoue. `Desinstaller.cmd` conserve les
profils et journaux.

Le chargeur prend en charge l'exécutable identifié par ce SHA-256 :

`5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77`

Une mise à jour du jeu peut nécessiter une adaptation du chargeur. Si une
alerte d'incompatibilité apparaît, ne pas lancer un monde moddé.

Dans **Modifier → MODS**, sélectionner un mod, l'activer et régler ses paramètres.
Les choix s'appliquent au lancement du monde. Après chargement de DLL, changer
de liste ou de monde peut demander une relance complète du jeu. Il n'y a pas
de déchargement à chaud ni de redémarrage automatique.

## Profils et paramètres

- `ModLoader/worlds/<GUID>.mods` contient les activations. S'il est absent,
  aucun mod n'est activé et les boutons Activer sont immédiatement disponibles.
- `ModLoader/worlds/<GUID>.settings-v2/<SHA256-identifiant-mod>.ini` contient
  les paramètres du mod pour ce monde, indépendamment de son activation.
- Supprimer un de ces fichiers réinitialise les choix correspondants.
  Les anciens fichiers `.settings` sont déplacés automatiquement à leur
  première lecture, sans remplacer un fichier existant.
- Sauvegarder tout `ModLoader/worlds` avec les mondes. Steam Cloud ne
  synchronise pas ces profils automatiquement. L'outil de transfert des
  profils `.mods` ne transporte pas encore les paramètres.

Les mods Test_A et Test_B 1.2 proposent des options oui/non, plusieurs choix
et un pourcentage. Ils ne produisent aucun effet de gameplay.

## Créer un mod

Commencer par le [guide du SDK](sdk/README.txt) et le
[standard V1 / ABI 1](sdk/STANDARD_V1.txt), qui constitue le contrat commun.

Chaque mod fournit une DLL Windows x64, un manifeste voisin `.dll.mod` et
les fonctions d'intégration du SDK. Un exemple compilable et un validateur
utilisant les mêmes contrôles que le chargeur sont inclus.

Les DLL d'autres frameworks ne sont pas automatiquement compatibles.
Une DLL native s'exécute avec les droits du jeu : le chargeur n'est pas
une sandbox.

## Vérifications

Après compilation, les scripts suivants couvrent les principaux contrôles :

- `test-mod-catalog.cmd`, `test-mod-profiles.cmd`, `test-mod-settings.cmd`.
- `test-new-world-profiles.cmd`, `test-mod-hardening.cmd`.
- `tools/audit-mod-067.cmd`, `tools/audit-mod-068.cmd`.
- `tools/analyze-loader.cmd` : analyse statique MSVC.
- `tools/test_install.ps1`, `tools/audit-install-format-068.ps1` :
  tests de l'installateur dans des dossiers isolés.

Certains tests nécessitent l'exécutable local du jeu et son emplacement attendu
sur cette machine. Ils ne constituent pas une suite portable sans adaptation.
Le dépôt ne redistribue aucun fichier du jeu.

Le fichier `.gitignore` utilise une liste explicite de fichiers publiables :
ajouter les nouveaux fichiers de développement à cette liste avant de les versionner.
