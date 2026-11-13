# Navigateur de mods — UI 0.3

Le bouton MODS de la fiche d'un monde ouvre un navigateur qui remplace
l'écran SessionStarter complet. Retour réaffiche le même écran natif avec
sa sélection inchangée. Aucun état du menu ou du monde n'est réécrit.

## Composants du jeu réutilisés

Adresses RVA uniquement pour le client SHA-256
`5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77`.

| Élément | Source native / comportement |
| --- | --- |
| Écran remplacé | `c5ef50`, doPlaySessionStarterV2, commun à la navigation des mondes et serveurs |
| Fenêtre | `a3f530`, plein écran, couche 9, mêmes drapeaux que l'écran natif |
| Marges | `d41360`, mêmes marges de sécurité que la page native |
| Colonnes | `a3c740`, lignes horizontales `a3cf70`, fermeture `a40080` |
| Largeurs | 1550 pour browserPanes (`1d920b0`) ; liste de 600 (`13d64e4`) |
| Détails | `d027e0`, panneau type 1, alpha 1 ; textures et transparence provenant du look du jeu |
| Entrées de liste | mêmes interactions que `cf8052..cf82c8`, surbrillance `d06ef0`, sélection `a47170` (événement 4) |
| Son de survol | `a56600`, style du jeu à `bundle+12620` |
| Titres | `b1da10` / `b25160`, style `bundle+11d44`, police caps `bundle+10bb8` |

La nouvelle page n'appelle pas la fonction qui peint un rectangle opaque
sur tout l'écran. Le décor pré-jeu déjà dessiné par le moteur reste visible.
Elle ne lance ni requête de serveurs ni connexion réseau.

Les structures ont été vérifiées dans le code du client : frame 0x23 octets
(tampon 0x30), texte jusqu'à 0x143 (tampon 0x150), style de ligne jusqu'à
0x58 (tampon 0x60), panneau jusqu'à 0xb4 (tampon 0xc0).
Le précédent tampon texte trop petit n'est pas réintroduit.

## Fonctionnalités

- Inventaire récursif de `Mods/`, extensions DLL insensibles à la casse.
- Lignes sélectionnables, nom du fichier et état Non chargé.
- Panneau de détail avec nom et chemin relatif de la DLL sélectionnée.
- Actualiser, Retour et pagination par six fichiers.
- État vide lorsque le dossier ne contient aucun mod.
- Aucun appel LoadLibrary sur les fichiers inventoriés ; RandomWorld exclu.

Le chargement de mods et les réglages d'activation par monde restent à réaliser.
Il ne s'agit pas d'un portage de toute la logique de recherche des serveurs :
seuls son enveloppe graphique et ses composants de liste sont réutilisés.

## Validation

Compilation x64 et `test-mod-catalog.cmd` réussis. La DLL installée correspond
à la compilation : `BA5CA5381FBA33D9C99160FF39B98BDA559DD162298439E55B409DE1150E243E`.
La capture automatique du jeu échoue (FrameArrived/window capture timeout).
Validation visuelle confirmée par l'utilisateur le 13 novembre 2026 :
« Oui, le rendu correspond » (image derrière, liste à gauche, détail translucide à droite).

Version fonctionnelle précédente conservée dans
`build/dinput8-before-native-browser.dll` et source dans
`build/loader-before-native-browser.cpp`.
