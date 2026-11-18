# Créer et distribuer un mod compatible

Ce guide correspond au chargeur **0.6.10**, au manifeste **V1** et à l’**ABI 1**.
Le [standard](STANDARD_V1.txt) définit les règles exactes ; le
[header public](include/enshrouded_mod.h) définit les signatures C.
Le dépôt est privé : son propriétaire doit donner accès aux développeurs qui
souhaitent le cloner. Les sources sont ici ; les paquets compilés prêts à
installer sont disponibles séparément dans les Releases du dépôt.

Le SDK permet à une DLL Windows x64 d’être découverte, configurée par monde et
initialisée par le chargeur. Il ne fournit pas d’API de création d’objets,
de recettes, de textures, de modèles 3D, de placement, de sauvegarde ou de réseau.
Ces fonctionnalités nécessitent un adaptateur moteur propre au mod, vérifié pour
la version du jeu prise en charge. Accepter un paquet ne valide pas son gameplay.

## 1. Obtenir et compiler le chargeur et le SDK

Prérequis : Windows x64, Git et Visual Studio 2022 Build Tools avec C++ x64 et
le SDK Windows. Les scripts racine utilisent cette installation précise :
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`.
Pour une autre édition ou installation, adapter le chemin de `vcvars64.bat` dans
les scripts concernés. MinHook est déjà fourni dans le dépôt.

Depuis une invite de commandes Windows (`cmd.exe`) :

```bat
git clone https://github.com/aebischer/Enshrouded-ModLoader.git
cd Enshrouded-ModLoader
build.cmd
build-sdk.cmd
test-mod-i18n.cmd
```

Arrêter à la première commande en échec. `build.cmd` fournit `dist\dinput8.dll`
et les objets nécessaires au validateur. `build-sdk.cmd` construit et teste :

| Sortie | Utilisation |
|---|---|
| `build\sdk-package\Mods\example\example_mod.dll` | Mod de démonstration sans effet de gameplay |
| `build\sdk-package\Mods\example\example_mod.dll.mod` | Manifeste de l’exemple |
| `build\validate_mod_package.exe` | Validation statique d’un paquet, sans exécuter ses DLL |
| `build\mod_sdk_test.exe` | Test du SDK en environnement temporaire, exécute l’exemple |

Le test C vérifie aussi que le header ABI est utilisable en C. Le helper de
traduction et l’exemple utilisent C++17. Aucun de ces scripts SDK n’installe de mod.
Le paquet du chargeur ne contient pas Test_A et Test_B : ces fixtures sont
réservées aux tests de développement sous `build/test-mods`.

## 2. Partir de l’exemple compilable

Pour un projet autonome, copier les fichiers suivants en conservant les chemins
relatifs (ou adapter `/I` dans le script de compilation) :

```text
MonProjet/
  include/
    enshrouded_mod.h          obligatoire : contrat ABI
    enshrouded_i18n.h         facultatif : catalogue multilingue C++17
  example/
    build.cmd
    example_mod.cpp
    example_mod.dll.mod
    localized_texts.h        facultatif : exemple de catalogue de textes
```

Ouvrir **x64 Native Tools Command Prompt for VS 2022**, puis :

```bat
cd C:\chemin\MonProjet
example\build.cmd C:\chemin\MonPaquet
```

Le script produit `MonPaquet\Mods\example\example_mod.dll` et son manifeste.
Les `.obj`, `.lib` et `.exp` sont des produits de compilation ; distribuer
uniquement le sous-dossier `Mods` et la documentation utile.

Pour créer votre identité, remplacer le dossier `example` de sortie et le nom
`example_mod` dans le script, puis renommer le manifeste de façon identique.
Exemple : `Mods\monstudio\monstudio_monmod.dll` accompagné de
`monstudio_monmod.dll.mod`. L’identifiant devient
`monstudio/monstudio_monmod.dll`. Ne pas le renommer après publication sans
prévoir la migration des profils et paramètres. Le nom de base de la DLL doit
être unique, même par rapport aux DLL installées dans d’autres sous-dossiers.

## 3. Déclarer le manifeste et les paramètres

Fichier texte **UTF-8**, extension exacte **`.dll.mod`** (pas `.dll.mod.txt`),
BOM facultatif, fins de ligne LF ou CRLF. Exemple complet :

```text
ENSHROUDED_PACKAGE_V1
kind=mod
version=1.0.0
abi=1
setting=messages|bool|Messages|true
setting=mode|choice|Mode|normal|slow|normal|fast
setting=intensity|percent|Intensity|100|0|200|25
text=en|setting.messages|Messages
text=fr|setting.messages|Messages de test
text=en|setting.mode|Mode
text=fr|setting.mode|Mode de fonctionnement
text=en|choice.mode.0|Slow
text=en|choice.mode.1|Normal
text=en|choice.mode.2|Fast
text=fr|choice.mode.0|Lent
text=fr|choice.mode.1|Normal
text=fr|choice.mode.2|Rapide
text=en|setting.intensity|Intensity
text=fr|setting.intensity|Intensité
```

| Type | Champs après `setting=` | Valeur transmise au mod |
|---|---|---|
| Booléen | `clé\|bool\|libellé\|défaut` | `true` ou `false` |
| Choix | `clé\|choice\|libellé\|défaut\|choix1\|choix2...` | Valeur interne exacte du choix |
| Pourcentage | `clé\|percent\|libellé\|défaut\|min\|max\|pas` | Entier sans `%`, par exemple `125` |

Maximum 16 paramètres ; clés `[a-z0-9_]` de 1 à 40 caractères ; 2 à 8 choix ;
pourcentages entre 0 et 10000 et 32 positions maximum. Un manifeste fait au plus
16 Kio. Les restrictions détaillées sur les champs sont dans le standard.
Ne pas inventer de clés `author`, `id`, `assets`, `side` ou `min_loader_version` :
elles ne sont pas acceptées. Indiquer ces informations dans le README du mod.

Les traductions changent l’affichage, jamais les clés ni les valeurs sauvegardées.
Le manifeste ne traduit que les paramètres. Voir [LOCALISATION.txt](LOCALISATION.txt)
pour les langues, les replis et le raccordement des textes d’objets au moteur.

## 4. Implémenter les exports et leur durée de vie

Utiliser les macros du header, sans changer les noms, les types ou le packing :

```cpp
ENSHROUDED_MOD_EXPORT int ENSHROUDED_MOD_CALL EnshroudedModConfigure(
    const char* world_id, const char* settings_utf8);

ENSHROUDED_MOD_EXPORT int ENSHROUDED_MOD_CALL EnshroudedModInitialize(
    const EnshroudedModContext* context);
```

`Initialize` est obligatoire. `Configure` est obligatoire dès qu’un paramètre
est déclaré ; elle est appelée avant `Initialize`. L’exemple fourni implémente
les deux. Si vous retirez tous les paramètres, adaptez aussi son contrôle
`configuredWorld` : le chargeur n’appellera plus `Configure`.

Le contexte de 24 octets fournit `size`, `abi_version`, `world_id` et `write_log`.
Vérifier les pointeurs, la taille et ABI 1 avant de les utiliser. Retourner
exactement `1` en cas de succès ou `0` en cas d’échec. Aucune exception C++ ne
doit franchir ces fonctions exportées.

`Configure` reçoit le GUID de 32 caractères et toutes les valeurs effectives
du monde, y compris les défauts. Pour le manifeste ci-dessus, les valeurs par
défaut sont transmises sous cette forme, avec des séparateurs LF :

```text
intensity=100
messages=true
mode=normal
```

Copier les données de `Configure` à conserver, puis les convertir en réglages
typés. Le contexte d’`Initialize` reste valide jusqu’à la fin du processus.
Ne pas modifier/libérer la mémoire du chargeur ni échanger des objets C++ ou
des allocations entre modules. `write_log` prend une chaîne UTF-8 terminée par
zéro ; le message est limité à 4096 octets.

L’initialisation n’est effectuée qu’une fois par processus, hors `DllMain`.
Il n’existe pas de callback SDK de mise à jour, de changement de monde, d’arrêt
ou de déchargement. Un mod qui installe des hooks doit gérer ses propres états
de session et ne pas utiliser des pointeurs d’objets après leur destruction.
Ne pas installer de hooks ni démarrer le gameplay dans `DllMain`.

## 5. Dépendances et ressources

Une bibliothèque tierce du paquet doit être une DLL x64 avec son manifeste :

```text
Mods/
  monstudio/
    monstudio_monmod.dll
    monstudio_monmod.dll.mod
    monstudio_support.dll
    monstudio_support.dll.mod
```

Dans le manifeste principal, ajouter :

```text
require=monstudio/monstudio_support.dll@1.0.0
```

Le manifeste de la bibliothèque contient :

```text
ENSHROUDED_PACKAGE_V1
kind=library
version=1.0.0
abi=1
```

Une bibliothèque n’a ni paramètres ni export `EnshroudedModInitialize`.
Elle est chargée automatiquement si requise. Une dépendance `kind=mod` doit
en revanche être activée explicitement. Déclarer tous les imports non système,
y compris différés ; versions exactes, pas de plages de versions. Ne pas
redistribuer les DLL système. L’exemple utilise `/MT` pour le runtime C++ statique.

Les fichiers de données annexes sont facultatifs et leur format appartient au
mod. Le chargeur n’interprète pas automatiquement des `.json`, `.png`, `.fbx`,
des recettes ou un dossier `assets`. Pour des ressources externes, votre code
doit résoudre le dossier de sa propre DLL (pas le répertoire courant du jeu),
les lire et les raccorder au moteur. Documenter leurs chemins et formats dans
votre paquet. Les ressources peuvent aussi être embarquées dans la DLL.
Il n’existe pas de format universel de ressource moteur dans ce SDK.

## 6. Valider puis installer

Depuis la racine du dépôt du chargeur, en utilisant le paquet construit :

```bat
build\validate_mod_package.exe C:\chemin\MonPaquet monstudio/monstudio_monmod.dll
```

Pour l’exemple sans renommage, utiliser `example/example_mod.dll`.
Le premier argument contient **Mods**, pas directement les DLL. Ajouter les
identifiants des autres mods activés/requis comme arguments supplémentaires.
Les bibliothèques sont résolues automatiquement. Codes retour : `0` valide,
`1` refusé, `2` arguments incorrects. Cette étape n’exécute aucune DLL du mod.

Fermer le jeu. Depuis le dépôt du chargeur, installer le proxy compilé :

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File install.ps1 -GamePath "D:\SteamLibrary\steamapps\common\Enshrouded"
```

Adapter le chemin. `Installer.cmd` permet aussi la détection du dossier Steam.
Un seul `dinput8.dll` peut occuper ce chemin : ne pas remplacer le proxy par
celui d’un autre framework. L’installateur du chargeur ne copie pas votre mod.
Copier ensuite le contenu du dossier `MonPaquet\Mods` dans `Enshrouded\Mods` :

```text
Enshrouded/
  enshrouded.exe
  dinput8.dll                       proxy unique du chargeur
  Mods/
    monstudio/
      monstudio_monmod.dll
      monstudio_monmod.dll.mod
  ModLoader/                       créé et géré par le chargeur
    mod-loader.log
    worlds/
      <GUID>.mods
      <GUID>.settings-v2/
        <SHA256-identifiant-mod>.ini
```

Ne pas créer `Mods\Mods`, ni mettre le mod dans `ModLoader`. Ne pas livrer
vos fichiers `worlds`, vos sauvegardes ou vos journaux. Le répertoire historique
`RandomWorld` utilisé par l’installateur contient ses reçus/restaurations ; ce
n’est pas un emplacement de mods et aucun mod de génération de monde n’est inclus.

Le client accepté par le chargeur a le SHA-256 suivant pour `enshrouded.exe` :

```text
5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77
```

Comparer avec `Get-FileHash -Algorithm SHA256` sous PowerShell. Une autre
version du jeu peut demander un portage du chargeur et de votre adaptateur.

Relancer le jeu, ouvrir **Modifier → MODS** pour le monde choisi, actualiser
si nécessaire, activer le mod et régler ses options. Lancer le monde pour
exécuter la DLL. La présence dans la liste ne signifie pas qu’elle a été chargée.
Les choix sont indépendants pour chaque monde. Après chargement de DLL, changer
la sélection peut nécessiter de quitter complètement puis relancer le jeu.
Il n’y a pas de déchargement à chaud. Remplacer un binaire jeu fermé uniquement.

## 7. Diagnostic et publication

| Symptôme | Vérification |
|---|---|
| Mod absent | Arborescence sous `Mods`, extension `.dll`, bouton Actualiser |
| Manifeste refusé | Nom exact `.dll.mod`, UTF-8, clés/types/valeurs, validateur |
| Chargement refusé | DLL x64, exports C, dépendances déclarées et versions exactes |
| Option invisible | Ligne `setting=`, limite 16, manifeste effectivement installé |
| Traduction absente | Chargeur 0.6.10+, clés `text`, langue du jeu, chaîne de repli |
| Réglages sans effet | Lecture dans `Configure`, relance demandée, bon monde activé |
| Mod listé mais sans effet | `Initialize` réussie, journal, adaptateur moteur implémenté |
| Redémarrage demandé | Fermer complètement le processus, puis relancer |

Consulter `Enshrouded\ModLoader\mod-loader.log` ; les messages envoyés à
`context->write_log` y apparaissent. Le stockage des réglages appartient au
chargeur, pas au mod. Un fichier absent rétablit les défauts ; un fichier invalide
provoque une erreur. Une modification incompatible du schéma exige une migration
documentée, il n’existe pas de callback de migration V1.

Avant diffusion :

1. Valider le paquet et toutes ses dépendances ; vérifier les exports avec
   `dumpbin /exports chemin\monstudio_monmod.dll` si nécessaire.
2. Tester en jeu deux mondes avec des réglages distincts, les valeurs limites,
   la désactivation, les redémarrages, la sauvegarde et le rechargement.
3. Tester français/anglais et les textes natifs séparément des libellés du SDK.
4. Fournir un README avec version du mod, chargeur requis, hash/version du jeu
   réellement testé, instructions, dépendances, limites et règles de mise à jour.
5. Distribuer l’arborescence `Mods` dans une archive, sans le proxy du chargeur,
   les fichiers du jeu, les objets de compilation ou les données personnelles.

Le serveur dédié et le multijoueur ne sont pas certifiés ; aucune API réseau
ni synchronisation automatique des réglages n’est fournie. Le SDK n’isole pas
les DLL : elles s’exécutent avec les droits du jeu. La validation statique ne
prouve ni leur innocuité ni l’absence de conflits entre hooks.
