# Glossaire FR - terminologie yaze (à figer pour cohérence)

Termes canoniques imposés à tous les traducteurs (agents inclus). Respecter la
casse et l'accentuation. En cas de doute, garder l'anglais plutôt qu'inventer.

## À conserver en anglais (termes techniques établis)
Tile, Tile16, Tile8, GFX, Sprite, Palette, Blockset, Tilemap, ROM, SNES, CGRAM,
VRAM, OAM, DMA, HDMA, PPU, APU, Asar, patch, overworld (nom propre du mode),
Mosaic, Bitmap, Atlas, Layer, Frame, Emulator (contexte technique).

## Traductions imposées
| Anglais | Français |
|---|---|
| File | Fichier |
| Edit | Édition |
| View | Affichage |
| Tools | Outils |
| Help | Aide |
| Settings | Paramètres |
| Preferences | Préférences |
| Appearance | Apparence |
| Language | Langue |
| Open | Ouvrir |
| Save | Enregistrer |
| Save As | Enregistrer sous |
| Close | Fermer |
| Cancel | Annuler |
| Apply | Appliquer |
| Undo | Annuler |
| Redo | Rétablir |
| Cut | Couper |
| Copy | Copier |
| Paste | Coller |
| Delete | Supprimer |
| Export | Exporter |
| Import | Importer |
| Dungeon | Donjon |
| Room | Salle |
| Entrance | Entrée |
| Exit | Sortie |
| Overworld | Monde (mode « Overworld » gardé si nom d'onglet technique) |
| Screen | Écran |
| Message / Text | Message / Texte |
| Graphics | Graphismes |
| Music | Musique |
| Object | Objet |
| Item | Objet (jeu) / Élément (UI) |
| Layer | Calque |
| Warning | Avertissement |
| Error | Erreur |
| Loading… | Chargement… |
| About | À propos |

## Règles de style
- Espace insécable avant `?! :;»` et après `«` (utiliser ` ` U+202F fine
  insécable si possible ; sinon espace simple).
- Ellipsis : utiliser `…` (U+2026), pas `...`.
- Guillemets : `« »` avec espaces fines, pas `"`.
- Conserver EXACTEMENT les spécificateurs de format (`%d %s %.2f`) dans le même
  ordre que la source (contrôlé par extract.py).
- Ne jamais traduire une clé contenant `##` (elle n'en contient jamais : ce sont
  des ID retirés à l'extraction).
- Impératif pour les actions de menu/bouton (« Enregistrer », pas
  « Enregistrer le fichier » sauf si la source le dit).
