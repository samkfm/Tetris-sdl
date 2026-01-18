// main.c changement
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define GRID_WIDTH 10
#define GRID_HEIGHT 20

// --------------------------------
// Variables globales
// --------------------------------
int grid[GRID_HEIGHT][GRID_WIDTH];
int currentPiece;
int pieceRot = 0, pieceX, pieceY;
int pieceColor[3];
int score = 0;   // <<< SCORE GLOBAL
char playerName[32] = "";

// Font + audio global
TTF_Font *gFont = NULL;
Mix_Music *gMusic = NULL;

// --------------------------------
// Tetrominos
// --------------------------------
int TETROMINOS[7][4][4] = {
    {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, // I
    {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}, // O
    {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, // T
    {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, // J
    {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, // L
    {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}, // S
    {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}  // Z
};

// ------------------------------------------------------------
// Retourne une cellule du Tetromino selon la rotation
// ------------------------------------------------------------
int pieceCell(int p,int r,int x,int y){
    int bx,by;
    switch(r&3){
        case 0: bx=x; by=y; break;
        case 1: bx=3-y; by=x; break;
        case 2: bx=3-x; by=3-y; break;
        case 3: bx=y; by=3-x; break;
    }
    return TETROMINOS[p][by][bx];
}

// ------------------------------------------------------------
// Détection collision
// ------------------------------------------------------------
int collision_at(int nx,int ny,int r){
    for(int y=0;y<4;y++)
        for(int x=0;x<4;x++)
            if(pieceCell(currentPiece,r,x,y)){
                int gx=nx+x, gy=ny+y;
                if(gx<0||gx>=GRID_WIDTH) return 1;
                if(gy>=GRID_HEIGHT) return 1;
                if(gy>=0 && grid[gy][gx]) return 1;
            }
    return 0;
}

// ------------------------------------------------------------
// Verrouille la pièce dans la grille
// ------------------------------------------------------------
void lockPiece(){
    for(int y=0;y<4;y++)
        for(int x=0;x<4;x++)
            if(pieceCell(currentPiece,pieceRot,x,y)){
                int gx=pieceX+x, gy=pieceY+y;
                if(gy>=0 && gy<GRID_HEIGHT && gx>=0 && gx<GRID_WIDTH){
                    int packed = (pieceColor[0]<<16)|(pieceColor[1]<<8)|pieceColor[2];
                    grid[gy][gx] = packed;
                }
            }
}

// ------------------------------------------------------------
// Suppression des lignes + ajout score (corrigée)
// Méthode : compacte la grille en copiant (bottom-up) les lignes non-pleines.
// ------------------------------------------------------------
void clearLines() {
    int writeRow = GRID_HEIGHT - 1;
    int linesRemoved = 0;

    for(int readRow = GRID_HEIGHT - 1; readRow >= 0; readRow--) {
        // check if readRow is full
        int full = 1;
        for(int x = 0; x < GRID_WIDTH; x++) {
            if(grid[readRow][x] == 0) { full = 0; break; }
        }

        if(full) {
            linesRemoved++;
            // skip copying this row (effectively remove it)
        } else {
            // copy readRow to writeRow (may be same)
            if(writeRow != readRow) {
                for(int x = 0; x < GRID_WIDTH; x++)
                    grid[writeRow][x] = grid[readRow][x];
            }
            writeRow--;
        }
    }

    // clear the remaining rows on top
    for(int y = writeRow; y >= 0; y--) {
        for(int x = 0; x < GRID_WIDTH; x++) grid[y][x] = 0;
    }

    // Score policy: conventional/simple (100 * number_of_lines)
    // you can change to classic Tetris scoring if you want
    if(linesRemoved > 0) {
        score += 100 * linesRemoved;
    }
}

// ------------------------------------------------------------
// Util : dessiner du texte (TTF)
// ------------------------------------------------------------
void renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {
    if(!font || !text) return;
    SDL_Color color = {255, 255, 0, 255}; // jaune
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if(!surf) {
        // fallback: nothing
        return;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if(!tex) { SDL_FreeSurface(surf); return; }
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

// ------------------------------------------------------------
// Dessine le score en haut à droite pendant la partie (TTF)
// ------------------------------------------------------------
void drawScore(SDL_Renderer *renderer, int winW, int winH) {
    if(!gFont) return;

    char buffer[128];
    sprintf(buffer, "%s - SCORE: %d", playerName, score);
    renderText(renderer, gFont, buffer, 20, 12);

    // taille font adaptative
    int fontW = winW / 32; if(fontW < 12) fontW = 12;
    TTF_Font *font = gFont;
    renderText(renderer, font, buffer, winW - 20 - (int)strlen(buffer)*fontW/2, 12);

}

// ------------------------------------------------------------
// ÉCRAN DES RÈGLES
// ------------------------------------------------------------
void rules_screen(SDL_Window *window, SDL_Renderer *renderer) {
    SDL_Event e;
    int winW, winH;
    int quit = 0;

    while(!quit) {
        SDL_GetWindowSize(window, &winW, &winH);

        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) exit(0);
            if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                quit = 1;
            if(e.type == SDL_MOUSEBUTTONDOWN)
                quit = 1;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if(gFont) {
            renderText(renderer, gFont, "RULES", winW/2 - 60, 40);

            renderText(renderer, gFont,
                "- Bouger - boutons DROITE / GAUCHE", 80, 140);
            renderText(renderer, gFont,
                "- Rotation - bouton du HAUT", 80, 190);
            renderText(renderer, gFont,
                "- Tomber doucement - bouton du BAS", 80, 240);
            renderText(renderer, gFont,
                "- Tomber rapidement - barre ESPACE", 80, 290);
            renderText(renderer, gFont,
                "- Le but est d'effacer les lignes pour avoir des points - 100 points par lignes effacés", 80, 340);
            renderText(renderer, gFont,
                "- Game over quand la toute la cage est remplie", 80, 390);

            renderText(renderer, gFont,
                "Cliquer n'importe où ou appuyer sur ESC", 80, winH - 80);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

void tableau_des_scores (SDL_Window *window, SDL_Renderer *renderer){

}

// ------------------------------------------------------------
// GAME OVER + AFFICHAGE SCORE (ASCII + texte TTF)
// ------------------------------------------------------------
void afficher_game_over(SDL_Renderer *renderer,int winW,int winH){
    SDL_SetRenderDrawColor(renderer,0,0,0,255);
    SDL_RenderClear(renderer);

    const char *lines[]={
        " ####   ###   ##   ##  ##### ",
        "##     ## ##  ### ###  ##    ",
        "## ### #####  ## # ##  ####  ",
        "##  ## ## ##  ##   ##  ##    ",
        " ####  ## ##  ##   ##  ##### ",
        "",
        " ###  ##    ##  #####  ##### ",
        "## ## ###  ###  ##     ##  ##",
        "## ##  ##  ##   ####   ##### ",
        "## ##   ####    ##     ##  ##",
        " ###     ##     #####  ##  ##"
    };
    int nb = sizeof(lines)/sizeof(lines[0]);
    int scale = winW/600; if(scale<2) scale=2;
    int charW=12*scale, charH=18*scale;
    int totalH=nb*charH;
    int startY=(winH-totalH)/2;

    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    for(int i=0;i<nb;i++){
        int len=(int)strlen(lines[i]);
        int startX=(winW-len*charW)/2;
        for(int j=0;j<len;j++){
            if(lines[i][j]!=' '){
                SDL_Rect r={startX+j*charW,startY+i*charH,charW-2,charH-2};
                SDL_RenderFillRect(renderer,&r);
            }
        }
    }

    // SCORE FINAL (TTF, plus propre)
    if(gFont) {
        char buf[128];
        sprintf(buf, "%s - FINAL SCORE : %d", playerName, score);
        renderText(renderer, gFont,buf, (winW - strlen(buf) * 18) / 2, startY - 60);
        // center text
        SDL_Color col = {255,255,0,255};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(gFont, buf, col);
        if(surf){
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            int tw = surf->w, th = surf->h;
            SDL_FreeSurface(surf);
            if(tex){
                SDL_Rect dst = { (winW - tw)/2, startY + totalH + 40, tw, th };
                SDL_RenderCopy(renderer, tex, NULL, &dst);
                SDL_DestroyTexture(tex);
            }
        }
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(3500);
    exit(0);
}
void ask_player_name(SDL_Window *window, SDL_Renderer *renderer) {
    SDL_Event e;
    int quit = 0;
    int winW, winH;

    playerName[0] = '\0';
    SDL_StartTextInput();

    while(!quit) {
        SDL_GetWindowSize(window, &winW, &winH);

        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) exit(0);

            if(e.type == SDL_TEXTINPUT) {
                if(strlen(playerName) < sizeof(playerName)-1) {
                    strcat(playerName, e.text.text);
                }
            }

            if(e.type == SDL_KEYDOWN) {
                if(e.key.keysym.sym == SDLK_BACKSPACE && strlen(playerName) > 0) {
                    playerName[strlen(playerName)-1] = '\0';
                }

                if(e.key.keysym.sym == SDLK_RETURN && strlen(playerName) > 0) {
                    quit = 1;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        renderText(renderer, gFont, "ENTER YOUR NAME", winW/2 - 180, winH/2 - 120);

        renderText(renderer, gFont, playerName, winW/2 - 180, winH/2 - 40);

        renderText(renderer, gFont, "Press ENTER to start", winW/2 - 180, winH/2 + 40);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
}

// ------------------------------------------------------------
// Génère une nouvelle pièce
// ------------------------------------------------------------
void spawn_new_piece(SDL_Renderer *renderer,int winW,int winH){
    currentPiece=rand()%7;
    pieceRot=0; pieceX=3; pieceY=-1;
    pieceColor[0]=rand()%200; pieceColor[1]=rand()%200; pieceColor[2]=rand()%200;

    if(collision_at(pieceX,pieceY,pieceRot)) afficher_game_over(renderer,winW,winH);
}

// ------------------------------------------------------------
// Rotation avec kicks
// ------------------------------------------------------------
int try_rotate_with_kick(int winW,int winH,SDL_Renderer *renderer){
    int newR=(pieceRot+1)&3;
    int kicks[]={0,-1,1,-2,2};
    for(int i=0;i<5;i++){
        int nx=pieceX+kicks[i];
        if(!collision_at(nx,pieceY,newR)){
            pieceX=nx; pieceRot=newR;
            return 1;
        }
    }
    return 0;
}

// ------------------------------------------------------------
// MENU PRINCIPAL (garde ton ASCII title/button)
// ------------------------------------------------------------
int menu(SDL_Window *window, SDL_Renderer *renderer, int winW, int winH){
    SDL_Event e;
    int start=0;

    const char *title[]={
        " ########  #####  ########  #####   ##  ###### ",
        "    ##     ##        ##     ##  ##  ##  ##     ",
        "    ##     #####     ##     #####   ##  ###### ",
        "    ##     ##        ##     ## ##   ##      ## ",
        "    ##     #####     ##     ##  ##  ##  ###### "
    };
    int titleNb=sizeof(title)/sizeof(title[0]);

    const char *playText[]={
        " ######  ##       ###   ###   ### ",
        "  ##  ##  ##      ## ##    ## ##    ",
        " #####   ##      #####     ##    ",
        " ##      ##      ## ##     ##    ",
        " ##      ######  ## ##     ##    "
    };
    int playNb=sizeof(playText)/sizeof(playText[0]);

    const char *rulesText[]={
        " #####     ##  ##  ##      #####  #### ",
        " ##  ##    ##  ##  ##      ##     ##   ",
        " #####     ##  ##  ##      ####   #### ",
        "  ##  ##    ##  ##  ##      ##       ##  ",
        " ##   ##    ####   ######  #####  #### "
    };
    int rulesNb=sizeof(rulesText)/sizeof(rulesText[0]);

    while(!start){
        SDL_GetWindowSize(window, &winW, &winH);

        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) exit(0);

            if(e.type==SDL_MOUSEBUTTONDOWN){
                int mx=e.button.x, my=e.button.y;
                int scale=winW/600; if(scale<2) scale=2;
                int charW=12*scale, charH=12*scale;

                // ---- PLAY bounds ----
                int maxPlayLen=0;
                for(int i=0;i<playNb;i++){
                    int len=(int)strlen(playText[i]);
                    if(len>maxPlayLen) maxPlayLen=len;
                }

                int playW=maxPlayLen*charW;
                int playH=playNb*charH;
                int playX=(winW-playW)/2;
                int playY=winH/2 + 60;

                // ---- RULES bounds ----
                int maxRulesLen=0;
                for(int i=0;i<rulesNb;i++){
                    int len=(int)strlen(rulesText[i]);
                    if(len>maxRulesLen) maxRulesLen=len;
                }

                int rulesW=maxRulesLen*charW;
                int rulesH=rulesNb*charH;
                int rulesX=(winW-rulesW)/2;
                int rulesY=playY + playH + 30;

                if(mx>=playX && mx<=playX+playW &&
                   my>=playY && my<=playY+playH){
                    start=1;
                    break;
                }

                if(mx>=rulesX && mx<=rulesX+rulesW &&
                   my>=rulesY && my<=rulesY+rulesH){
                    rules_screen(window, renderer);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);

        int scale=winW/600; if(scale<2) scale=2;
        int charW=12*scale,charH=18*scale;

        int maxLen=0;
        for(int i=0;i<titleNb;i++){
            int len=(int)strlen(title[i]);
            if(len>maxLen) maxLen=len;
        }

        int totalTitleW=maxLen*charW;
        int totalTitleH=titleNb*charH;
        int titleStartX=(winW-totalTitleW)/2;
        int titleStartY=winH/4;

        // ---- TITLE ----
        for(int i=0;i<titleNb;i++){
            int len=(int)strlen(title[i]);
            int startX=titleStartX + (maxLen-len)/2*charW;
            for(int j=0;j<len;j++){
                if(title[i][j]!=' '){
                    int flicker=rand()%50;
                    SDL_SetRenderDrawColor(renderer,205+flicker,205+flicker,255,255);
                    SDL_Rect r={startX+j*charW,titleStartY+i*charH,charW-2,charH-2};
                    SDL_RenderFillRect(renderer,&r);
                }
            }
        }

        // ---- PLAY BUTTON ----
        int btnCharW=12*scale, btnCharH=12*scale;

        int maxPlayLen=0;
        for(int i=0;i<playNb;i++){
            int len=(int)strlen(playText[i]);
            if(len>maxPlayLen) maxPlayLen=len;
        }

        int playW=maxPlayLen*btnCharW;
        int playH=playNb*btnCharH;
        int playX=(winW-playW)/2;
        int playY=titleStartY + totalTitleH + 40;

        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX,&mouseY);

        int hoverPlay = (mouseX>=playX && mouseX<=playX+playW &&
                         mouseY>=playY && mouseY<=playY+playH);

        SDL_SetRenderDrawColor(renderer, hoverPlay?150:100, hoverPlay?150:100,255,255);
        SDL_Rect btnPlay={playX,playY,playW,playH};
        SDL_RenderFillRect(renderer,&btnPlay);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderDrawRect(renderer,&btnPlay);

        for(int i=0;i<playNb;i++){
            int len=(int)strlen(playText[i]);
            int sx=playX + (playW-len*btnCharW)/2;
            for(int j=0;j<len;j++){
                if(playText[i][j]=='#'){
                    SDL_Rect r={sx+j*btnCharW,playY+i*btnCharH,
                                btnCharW-2,btnCharH-2};
                    SDL_RenderFillRect(renderer,&r);
                }
            }
        }

        // ---- RULES BUTTON ----
        int maxRulesLen=0;
        for(int i=0;i<rulesNb;i++){
            int len=(int)strlen(rulesText[i]);
            if(len>maxRulesLen) maxRulesLen=len;
        }

        int rulesW=maxRulesLen*btnCharW;
        int rulesH=rulesNb*btnCharH;
        int rulesX=(winW-rulesW)/2;
        int rulesY=playY + playH + 30;

        int hoverRules = (mouseX>=rulesX && mouseX<=rulesX+rulesW &&
                          mouseY>=rulesY && mouseY<=rulesY+rulesH);

        SDL_SetRenderDrawColor(renderer, hoverRules?150:100, hoverRules?150:100,255,255);
        SDL_Rect btnRules={rulesX,rulesY,rulesW,rulesH};
        SDL_RenderFillRect(renderer,&btnRules);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderDrawRect(renderer,&btnRules);

        for(int i=0;i<rulesNb;i++){
            int len=(int)strlen(rulesText[i]);
            int sx=rulesX + (rulesW-len*btnCharW)/2;
            for(int j=0;j<len;j++){
                if(rulesText[i][j]=='#'){
                    SDL_Rect r={sx+j*btnCharW,rulesY+i*btnCharH, btnCharW-2,btnCharH-2};
                    SDL_RenderFillRect(renderer,&r);
                }
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(30);
    }

    return 1;
}

// ------------------------------------------------------------
// ------------------------------ MAIN -------------------------
// ------------------------------------------------------------
int main(int argc,char *argv[]){
    srand((unsigned)time(NULL)); //initialise le générateur de nombres aléatoires pour faire des pièces aléatoire + couleurs

    // --------------------
    // SDL INIT + SDL_MIXER + TTF
    // --------------------
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){ //initialise la vidéo (fenêtre) + audio
        printf("Erreur SDL : %s\n", SDL_GetError());
        return 1;
    }

    // init ttf
    if(TTF_Init() != 0){ //initialisation du système de polices ttf
        printf("Erreur TTF_Init: %s\n", TTF_GetError());
        // le jeu continue mais sans texte 
    }


    gFont = TTF_OpenFont("PixelTetris.ttf", 40); //charge la police depis le fichier dans l'ordinateur + taille de base 40
    if(!gFont) {
        printf("Warning: impossible de charger PixelTetris.ttf : %s\n", TTF_GetError());
        // le jeu continue sans texte 
    }

    // init mixer (with mp3 support attempt)
    int mixFlags = MIX_INIT_MP3; //demande le support mp3
    if((Mix_Init(mixFlags) & mixFlags) != mixFlags) { //vérifie si le support mp3 est disponible
        // message d'avertissement mais le jeu continue
        printf("Warning: Mix_Init failed for MP3: %s\n", Mix_GetError());
    }

    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0){ //configure l'audio avec fréquence=44100 Hz, stéréo (2)(ce choix car son plus réaliste gauche droite), buffer = 2048 (stocke le son avant de l'envoyer aux haut-parleurs)
        printf("Erreur audio (Mix_OpenAudio) : %s\n", Mix_GetError());
        // On continue sans son si impossible
    }

    gMusic = Mix_LoadMUS("tetris.mp3"); //charge la musique de fond
    if(!gMusic){
        printf("Erreur Mix_LoadMUS : %s\n", Mix_GetError()); // si la musique est introuvable : message 
    } else {
        if(Mix_PlayMusic(gMusic, -1) == -1) { // joue la musique en boucle
            printf("Erreur Mix_PlayMusic : %s\n", Mix_GetError());
        }
    }

    // Fenêtre
    SDL_Window *window = SDL_CreateWindow("Tetris", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 800, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE); // création d'une fenêtre (position centré, taille : 640 x 800, redimensionnable)
    if(!window){ //si la fenêtre n'est pas crée on fait un nettoyage complet (musique, audio, police et SDL) + on sort du programme
        printf("Erreur fenetre: %s\n", SDL_GetError());
        if(gMusic) Mix_FreeMusic(gMusic);
        Mix_CloseAudio();
        Mix_Quit();
        if(gFont) TTF_CloseFont(gFont);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); //crée le moteur de rendu graphique (accélération GPU et synchronisation verticale (évite le tearing  plus fluide))
    if(!renderer){ // si renderer échoue nettoyage + arrêt du programme
        printf("Erreur renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        if(gMusic) Mix_FreeMusic(gMusic);
        Mix_CloseAudio();
        Mix_Quit();
        if(gFont) TTF_CloseFont(gFont);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    int winW=640, winH=800; //stocke la largeur et la longueur de la fenêtre actuelles
    SDL_GetWindowSize(window,&winW,&winH); //lit la taille actuelle de la fenêtre même après redimensionnement 

    menu(window, renderer, winW, winH);  // affiche le menu principal + attend que le joueur clique sur play 
    ask_player_name(window, renderer); //demande le nom du joueur et le stocke dans playerName 
    memset(grid,0,sizeof(grid)); //mets toute la grille à zéro et vide le plateau 
    spawn_new_piece(renderer,winW,winH); //génère la première pièce et vérifie le game over immédiat

    Uint32 lastFall=SDL_GetTicks(); //sauegarde le temps actuel (en ms), sert à calculer la vitesse de chute 
    int quit=0; // condition de sortie 
    SDL_Event e; //évenements clavier / souris 

    while(!quit){ //s'execute tant que le joueur ne quitte pas 
        SDL_GetWindowSize(window,&winW,&winH); // permet un rendu adaptatif

        while(SDL_PollEvent(&e)){ //récupère tous les événements SDL
            if(e.type==SDL_QUIT){ quit=1; break; } //clic sue la croix : sortie 

            if(e.type==SDL_KEYDOWN){ //détecte une touche pressée
                switch(e.key.keysym.sym){
                    case SDLK_LEFT: // gauche : déplacement si collision 
                        if(!collision_at(pieceX-1,pieceY,pieceRot)) pieceX--;
                        break;

                    case SDLK_RIGHT: //droite : déplacement si collision 
                        if(!collision_at(pieceX+1,pieceY,pieceRot)) pieceX++;
                        break;

                    case SDLK_DOWN: //bas : déplacement si collision 
                        if(!collision_at(pieceX,pieceY+1,pieceRot)) pieceY++;
                        break;

                    case SDLK_UP: //haut : rotation avec correction murale 
                        try_rotate_with_kick(winW,winH,renderer);
                        break;

                    case SDLK_SPACE: //chute instantanée puis verrouillage + nouvelle pièce 
                        while(!collision_at(pieceX,pieceY+1,pieceRot)) pieceY++;
                        lockPiece();
                        clearLines();
                        spawn_new_piece(renderer,winW,winH);
                        lastFall=SDL_GetTicks();
                        break;
                }
            }
        }
        int fallDelay = 500 - (score / 500) * 50; // chaque fois  qu'il y a 500 points on augmente la vitesse de 100ms 
        if (fallDelay < 100) fallDelay = 100;  // limite minimale : 100ms

        if(SDL_GetTicks()-lastFall>fallDelay){   // definir le temps que les pièces descendent / si assez de temps écoulé : pièce descend ou se verrouille
            if(!collision_at(pieceX,pieceY+1,pieceRot)) pieceY++; 
            else{
                lockPiece();
                clearLines();
                spawn_new_piece(renderer,winW,winH);
            }
            lastFall=SDL_GetTicks();
        }

        float tile=(winH/(float)GRID_HEIGHT < winW/(float)GRID_WIDTH)? winH/(float)GRID_HEIGHT : winW/(float)GRID_WIDTH; // calcule la taille d'une case / s'adapte à la fenêtre 

        float offsetX=(winW - tile*GRID_WIDTH)/2.0f; //centre la grille 
        float offsetY=(winH - tile*GRID_HEIGHT)/2.0f;

        SDL_SetRenderDrawColor(renderer,0,0,0,255); // couleur de fond
        SDL_RenderClear(renderer); //efface l'écran avec la couleur définie juste avant

        // Grille
        for(int gy=0; gy<GRID_HEIGHT; gy++){
            for(int gx=0; gx<GRID_WIDTH; gx++){
                SDL_Rect cell={
                    (int)(offsetX+gx*tile),
                    (int)(offsetY+gy*tile),
                    (int)(tile+0.5f),
                    (int)(tile+0.5f)
                };

                if(grid[gy][gx]){
                    int packed=grid[gy][gx];
                    int r=(packed>>16)&0xFF, g=(packed>>8)&0xFF, b=packed&0xFF;
                    SDL_SetRenderDrawColor(renderer,r,g,b,255); //couleur aléatoire pour les pièces 
                    SDL_RenderFillRect(renderer,&cell); //dessine un rectangle plein à la position et taille décrites par cell

                    // dessine une bordure fine autour du bloc
                    SDL_SetRenderDrawColor(renderer,0,0,0,200);
                    SDL_RenderDrawRect(renderer,&cell);
                } else {
                    SDL_SetRenderDrawColor(renderer,50,50,50,255);
                    SDL_RenderDrawRect(renderer,&cell);
                }
            }
        }

        // Pièce active
        int rcol=pieceColor[0], gcol=pieceColor[1], bcol=pieceColor[2];
        SDL_SetRenderDrawColor(renderer,rcol,gcol,bcol,255);

        for(int y=0;y<4;y++)
            for(int x=0;x<4;x++)
                if(pieceCell(currentPiece,pieceRot,x,y)){
                    int gx=pieceX+x, gy=pieceY+y;
                    // only draw visible cells (gy might be negative)
                    if(gy >= 0 && gy < GRID_HEIGHT){
                        SDL_Rect cell={
                            (int)(offsetX+gx*tile),
                            (int)(offsetY+gy*tile),
                            (int)(tile+0.5f),
                            (int)(tile+0.5f)
                        };
                        SDL_RenderFillRect(renderer,&cell);
                        SDL_SetRenderDrawColor(renderer,0,0,0,255);
                        SDL_RenderDrawRect(renderer,&cell);
                        SDL_SetRenderDrawColor(renderer,rcol,gcol,bcol,255);
                    }
                }

        // Affiche le score pendant la partie (TTF)
        drawScore(renderer, winW, winH); 

        SDL_RenderPresent(renderer); //affiche tout l'écran 
        SDL_Delay(8); //petite pause pour stabiliser le framerate (nombre d'images affichées par seconde)
    }

    // Nettoyage audio + ttf
    if(gMusic) Mix_FreeMusic(gMusic);
    Mix_CloseAudio();
    Mix_Quit();

    if(gFont) TTF_CloseFont(gFont);
    TTF_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
} 