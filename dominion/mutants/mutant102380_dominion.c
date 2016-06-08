#include "dominion.h"
#include "dominion_helpers.h"
#include "rngs.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h> //for memset

int compare(const void* a, const void* b) {
	if (*(int*)a > *(int*)b)
		return 1;
	if (*(int*)a < *(int*)b)
		return -1;
	return 0;
}

struct gameState* newGame() {
	struct gameState* g = malloc(sizeof(struct gameState));
	return g;
}

int* kingdomCards(int k1, int k2, int k3, int k4, int k5, int k6, int k7,
	int k8, int k9, int k10) {
	int* k = malloc(10 * sizeof(int));
	k[0] = k1;
	k[1] = k2;
	k[2] = k3;
	k[3] = k4;
	k[4] = k5;
	k[5] = k6;
	k[6] = k7;
	k[7] = k8;
	k[8] = k9;
	k[9] = k10;
	return k;
}

int initializeGame(int numPlayers, int kingdomCards[10], int randomSeed,
struct gameState *state) {

	int i;
	int j;
	int it;
	//set up random number generator
	SelectStream(1);
	PutSeed((long)randomSeed);

	//check number of players
	if (numPlayers > MAX_PLAYERS || numPlayers < 2)
	{
		return -1;
	}

	//set number of players
	state->numPlayers = numPlayers;

	//check selected kingdom cards are different
	for (i = 0; i < 10; i++)
	{
		for (j = 0; j < 10; j++)
		{
			if (j != i && kingdomCards[j] == kingdomCards[i])
			{
				return -1;
			}
		}
	}


	//initialize supply
	///////////////////////////////

	//set number of Curse cards
	if (numPlayers == 2)
	{
		state->supplyCount[curse] = 10;
	}
	else if (numPlayers == 3)
	{
		state->supplyCount[curse] = 20;
	}
	else
	{
		state->supplyCount[curse] = 30;
	}

	//set number of Victory cards
	if (numPlayers == 2)
	{
		state->supplyCount[estate] = 8;
		state->supplyCount[duchy] = 8;
		state->supplyCount[province] = 8;
	}
	else
	{
		state->supplyCount[estate] = 12;
		state->supplyCount[duchy] = 12;
		state->supplyCount[province] = 12;
	}

	//set number of Treasure cards
	state->supplyCount[copper] = 60 - (7 * numPlayers);
	state->supplyCount[silver] = 40;
	state->supplyCount[gold] = 30;

	//set number of Kingdom cards
	for (i = adventurer; i <= treasure_map; i++)          //loop all cards
	{
		for (j = 0; j < 10; j++)                  //loop chosen cards
		{
			if (kingdomCards[j] == i)
			{
				//check if card is a 'Victory' Kingdom card
				if (kingdomCards[j] == great_hall || kingdomCards[j] == gardens)
				{
					if (numPlayers == 2) {
						state->supplyCount[i] = 8;
					}
					else { state->supplyCount[i] = 12; }
				}
				else
				{
					state->supplyCount[i] = 10;
				}
				break;
			}
			else    //card is not in the set choosen for the game
			{
				state->supplyCount[i] = -1;
			}
		}

	}

	////////////////////////
	//supply intilization complete

	//set hands, decks, discards to -1
	for (i = 0; i < MAX_PLAYERS; i++) {
		memset(state->hand[i], -1, MAX_HAND);
		memset(state->deck[i], -1, MAX_DECK);
		memset(state->discard[i], -1, MAX_DECK);
		state->handCount[i] = 0;
		state->deckCount[i] = 0;
		state->discardCount[i] = 0;
	}

	//set player decks
	for (i = 0; i < numPlayers; i++)
	{
		state->deckCount[i] = 0;
		for (j = 0; j < 3; j++)
		{
			state->deck[i][j] = estate;
			state->deckCount[i]++;
		}
		for (j = 3; j < 10; j++)
		{
			state->deck[i][j] = copper;
			state->deckCount[i]++;
		}
	}

	//shuffle player decks
	for (i = 0; i < numPlayers; i++)
	{
		if (shuffle(i, state) < 0)
		{
			return -1;
		}
	}

	//draw player hands
	for (i = 0; i < numPlayers; i++)
	{
		//initialize hand size to zero
		state->handCount[i] = 0;
		state->discardCount[i] = 0;
	}

	//set embargo tokens to 0 for all supply piles
	for (i = 0; i <= treasure_map; i++)
	{
		state->embargoTokens[i] = 0;
	}

	//initialize first player's turn
	state->outpostPlayed = 0;
	state->phase = 0;
	state->numActions = 1;
	state->numBuys = 1;
	state->playedCardCount = 0;
	state->whoseTurn = 0;
	state->handCount[state->whoseTurn] = 0;

	//Moved draw cards to here, only drawing at the start of a turn
	for (i = 0; i < state->numPlayers; i++) {
		for (it = 0; it < 5; it++) {
			drawCard(i, state);
		}
	}

	updateCoins(state->whoseTurn, state, 0);

	return 0;
}

int shuffle(int player, struct gameState *state) {


	int newDeck[MAX_DECK];
	int newDeckPos = 0;
	int card;
	int i;

	if (state->deckCount[player] < 1)
		return -1;
	qsort((void*)(state->deck[player]), state->deckCount[player], sizeof(int), compare);
	/* SORT CARDS IN DECK TO ENSURE DETERMINISM! */

	while (state->deckCount[player] > 0) {
		card = floor(Random() * state->deckCount[player]);
		newDeck[newDeckPos] = state->deck[player][card];
		newDeckPos++;
		for (i = card; i < state->deckCount[player] - 1; i++) {
			state->deck[player][i] = state->deck[player][i + 1];
		}
		state->deckCount[player]--;
	}
	for (i = 0; i < newDeckPos; i++) {
		state->deck[player][i] = newDeck[i];
		state->deckCount[player]++;
	}

	return 0;
}

int playedCard(int handPos, int* choice1, int* choice2, struct gameState *state) {
	//add the card to the played cards field
	int who = state->whoseTurn;
	state->playedCards[state->playedCardCount] = state->hand[who][handPos];
	state->playedCardCount++;

	//we're gonna move the last card to the hand position, so we need to adjust choices accordingly
	if (choice1 != NULL && *choice1 == state->handCount[who] - 1)
		*choice1 = handPos;
	if (choice2 != NULL && *choice2 == state->handCount[who] - 1)
		*choice2 = handPos;

	state->hand[who][handPos] = state->hand[who][state->handCount[who] - 1];
	state->hand[who][state->handCount[who] - 1] = -1;
	state->handCount[who]--;
	return 0;
}

int endPlayed(struct gameState *state, int trashFlag) {
	//add cards from played pile to discard pile
	int who = state->whoseTurn;
	int i;
	for (i = state->playedCardCount - 1; i >= 0; i--) {
		if (trashFlag == 0) {
			state->discard[who][state->discardCount[who]] = state->playedCards[i];
			state->discardCount[who]++;
		}
		state->playedCards[i] = -1;
		state->playedCardCount--;
	}
	return 0;
}

int playCard(int handPos, int choice1, int choice2, int choice3, struct gameState *state)
{
	int card;
	int coin_bonus = 0;       //tracks coins gain from actions

	//check if it is the right phase
	if (state->phase != 0)
	{
		return -1;
	}

	//check if player has enough actions
	if (state->numActions < 1)
	{
		return -1;
	}

	if (handPos >= state->handCount[state->whoseTurn]) {
		return -1;
	}

	//get card played
	card = handCard(handPos, state);

	//check if selected card is an action
	if (card < adventurer || card > treasure_map)
	{
		return -1;
	}

	//play card
	if (cardEffect(card, choice1, choice2, choice3, state, handPos, &coin_bonus) < 0)
	{
		return -1;
	}

	//reduce number of actions
	state->numActions--;

	//update coins (Treasure cards may be added with card draws)
	updateCoins(state->whoseTurn, state, coin_bonus);

	return 0;
}

int buyCard(int supplyPos, struct gameState *state) {
	int who, i;
	if (DEBUG) {
		printf("Entering buyCard...\n");
	}

	who = state->whoseTurn;

	if (state->numBuys < 1) {
		if (DEBUG)
			printf("You do not have any buys left\n");
		return -1;
	}
	if (supplyPos < curse || supplyPos > treasure_map) {
		return -1;
	}
	if (supplyCount(supplyPos, state) < 1) {
		if (DEBUG)
			printf("There are not any of that type of card left\n");
		return -1;
	}
	if (state->coins < getCost(supplyPos)) {
		if (DEBUG)
			printf("You do not have enough money to buy that. You have %d coins.\n", state->coins);
		return -1;
	}
	
	state->phase = 1;
	gainCard(supplyPos, state, 0, who); 
	for (i = 0; i < state->embargoTokens[supplyPos]; i++)
		gainCard(curse, state, 0, who);

	state->coins = (state->coins) - (getCost(supplyPos));
	state->numBuys--;
	if (DEBUG)
		printf("You bought card number %d for %d coins. You now have %d buys and %d coins.\n", supplyPos, getCost(supplyPos), state->numBuys, state->coins);
	
	return 0;
}

int numHandCards(struct gameState *state) {
	return state->handCount[whoseTurn(state)];
}

int handCard(int handPos, struct gameState *state) {
	int currentPlayer = whoseTurn(state);
	return state->hand[currentPlayer][handPos];
}

int supplyCount(int card, struct gameState *state) {
	return state->supplyCount[card];
}

int fullDeckCount(int player, int card, struct gameState *state) {
	int i;
	int count = 0;

	for (i = 0; i < state->deckCount[player]; i++)
	{
		if (state->deck[player][i] == card) count++;
	}

	for (i = 0; i < state->handCount[player]; i++)
	{
		if (state->hand[player][i] == card) count++;
	}

	for (i = 0; i < state->discardCount[player]; i++)
	{
		if (state->discard[player][i] == card) count++;
	}

	return count;
}

int whoseTurn(struct gameState *state) {
	return state->whoseTurn;
}

int endTurn(struct gameState *state) {
	int k;
	int i;
	int currentPlayer = whoseTurn(state);

	//Discard hand
	for (i = 0; i < state->handCount[currentPlayer]; i++) {
		state->discard[currentPlayer][state->discardCount[currentPlayer]++] = state->hand[currentPlayer][i];//Discard
		state->hand[currentPlayer][i] = -1;//Set card to -1
	}
	state->handCount[currentPlayer] = 0;//Reset hand count

	if (state->outpostPlayed == 1) {
		//draw 3 cards
		for (k = 0; k < 3; k++) {
			drawCard(state->whoseTurn, state);//Draw a card
		}
		state->outpostTurn = 1;
	}
	else {
		//draw 5 cards
		for (k = 0; k < 5; k++) {
			drawCard(state->whoseTurn, state);//Draw a card
		}

		//Code for determining the player
		if (currentPlayer < (state->numPlayers - 1)) {
			state->whoseTurn = currentPlayer + 1;//Still safe to increment
		}
		else {
			state->whoseTurn = 0;//Max player has been reached, loop back around to player 1
		}
		state->outpostTurn = 0;
		endPlayed(state, 0); //needed to get rid of outpost on second turn
		state->playedCardCount = 0;
	}
	
	state->outpostPlayed = 0;
	state->phase = 0;
	state->numActions = 1;
	state->coins = 0;
	state->numBuys = 1;

	//Update money
	updateCoins(state->whoseTurn, state, 0);

	return 0;
}

int isGameOver(struct gameState *state) {
	int i;
	int j;

	//if stack of Province cards is empty, the game ends
	if (state->supplyCount[province] == 0)
	{
		return 1;
	}

	//if three supply pile are at 0, the game ends
	j = 0;
	for (i = 0; i < 25; i++)
	{
		if (state->supplyCount[i] == 0)
		{
			j++;
		}
	}
	if (j >= 3)
	{
		return 1;
	}

	return 0;
}

int scoreFor(int player, struct gameState *state) {

	int i;
	int score = 0;
	//score from hand
	for (i = 0; i < state->handCount[player]; i++)
	{
		if (state->hand[player][i] == curse) { score = score - 1; };
		if (state->hand[player][i] == estate) { score = score + 1; };
		if (state->hand[player][i] == duchy) { score = score + 3; };
		if (state->hand[player][i] == province) { score = score + 6; };
		if (state->hand[player][i] == great_hall) { score = score + 1; };
		if (state->hand[player][i] == gardens) { score = score + (fullDeckCount(player, 0, state) / 10); };
	}

	//score from discard
	for (i = 0; i < state->discardCount[player]; i++)
	{
		if (state->discard[player][i] == curse) { score = score - 1; };
		if (state->discard[player][i] == estate) { score = score + 1; };
		if (state->discard[player][i] == duchy) { score = score + 3; };
		if (state->discard[player][i] == province) { score = score + 6; };
		if (state->discard[player][i] == great_hall) { score = score + 1; };
		if (state->discard[player][i] == gardens) { score = score + (fullDeckCount(player, 0, state) / 10); };
	}

	//score from deck
	for (i = 0; i < state->deckCount[player]; i++)
	{
		if (state->deck[player][i] == curse) { score = score - 1; };
		if (state->deck[player][i] == estate) { score = score + 1; };
		if (state->deck[player][i] == duchy) { score = score + 3; };
		if (state->deck[player][i] == province) { score = score + 6; };
		if (state->deck[player][i] == great_hall) { score = score + 1; };
		if (state->deck[player][i] == gardens) { score = score + (fullDeckCount(player, 0, state) / 10); };
	}

	return score;
}

int getWinners(int players[MAX_PLAYERS], struct gameState *state) {
	int i;
	int j;
	int highScore;
	int currentPlayer;

	//get score for each player
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		//set unused player scores to -9999
		if (i >= state->numPlayers)
		{
			players[i] = -9999;
		}
		else
		{
			players[i] = scoreFor(i, state);
		}
	}

	//find highest score
	j = 0;
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if (players[i] > players[j])
		{
			j = i;
		}
	}
	highScore = players[j];

	//add 1 to players who had less turns
	currentPlayer = whoseTurn(state);
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if (players[i] == highScore && i > currentPlayer)
		{
			players[i]++;
		}
	}

	//find new highest score
	j = 0;
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if (players[i] > players[j])
		{
			j = i;
		}
	}
	highScore = players[j];

	//set winners in array to 1 and rest to 0
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if (players[i] == highScore)
		{
			players[i] = 1;
		}
		else
		{
			players[i] = 0;
		}
	}

	return 0;
}

int drawCard(int player, struct gameState *state)
{
	int count;
	int deckCounter;
	if (state->deckCount[player] <= 0) {//Deck is empty

	  //Step 1 Shuffle the discard pile back into a deck
		int i;
		//Move discard to deck
		for (i = 0; i < state->discardCount[player]; i++) {
			state->deck[player][i] = state->discard[player][i];
			state->discard[player][i] = -1;
		}

		state->deckCount[player] = state->discardCount[player];
		state->discardCount[player] = 0;//Reset discard

		//Shufffle the deck
		shuffle(player, state);//Shuffle the deck up and make it so that we can draw

		if (DEBUG) {//Debug statements
			printf("Deck count now: %d\n", state->deckCount[player]);
		}

		state->discardCount[player] = 0;

		//Step 2 Draw Card
		count = state->handCount[player];//Get current player's hand count

		if (DEBUG) {//Debug statements
			printf("Current hand count: %d\n", count);
		}

		deckCounter = state->deckCount[player];//Create a holder for the deck count

		if (deckCounter == 0)
			return -1;

		state->hand[player][count] = state->deck[player][deckCounter - 1];//Add card to hand
		state->deck[player][deckCounter - 1] = -1;
		state->deckCount[player]--;
		state->handCount[player]++;//Increment hand count
	}

	else {
		int count = state->handCount[player];//Get current hand count for player
		int deckCounter;
		if (DEBUG) {//Debug statements
			printf("Current hand count: %d\n", count);
		}

		deckCounter = state->deckCount[player];//Create holder for the deck count
		state->hand[player][count] = state->deck[player][deckCounter - 1];//Add card to the hand
		state->deck[player][deckCounter - 1] = -1;
		state->deckCount[player]--;
		state->handCount[player]++;//Increment hand count
	}

	return 0;
}

int getCost(int cardNumber)
{
	switch (cardNumber)
	{
	case curse:
		return 0;
	case estate:
		return 2;
	case duchy:
		return 5;
	case province:
		return 8;
	case copper:
		return 0;
	case silver:
		return 3;
	case gold:
		return 6;
	case adventurer:
		return 6;
	case council_room:
		return 5;
	case feast:
		return 4;
	case gardens:
		return 4;
	case mine:
		return 5;
	case remodel:
		return 4;
	case smithy:
		return 4;
	case village:
		return 3;
	case baron:
		return 4;
	case great_hall:
		return 3;
	case minion:
		return 5;
	case steward:
		return 3;
	case tribute:
		return 5;
	case ambassador:
		return 3;
	case cutpurse:
		return 4;
	case embargo:
		return 2;
	case outpost:
		return 5;
	case salvager:
		return 4;
	case sea_hag:
		return 4;
	case treasure_map:
		return 4;
	}

	return -1;
}

int smithyEffect(int currentPlayer, int handPos, struct gameState *state) {
	int i;
	//+3 Cards
	playedCard(handPos, NULL, NULL, state);
	for (i = 0; i < 3; i++) {
		drawCard(currentPlayer, state);
	}

	endPlayed(state, 0);
	return 0;
}

int villageEffect(int currentPlayer, int handPos, struct gameState *state) {
	//+1 Card
	playedCard(handPos, NULL, NULL, state);
	drawCard(currentPlayer, state);

	//+2 Actions
	state->numActions += state->numActions + 2;

	//discard played card from hand
	endPlayed(state, 0);
	return 0;
}

int greatHallEffect(int currentPlayer, int handPos, struct gameState *state) {
	//+1 Card
	playedCard(handPos, NULL, NULL, state);
	drawCard(currentPlayer, state);

	//+1 Actions
	state->numActions++;

	//discard card from hand
	endPlayed(state, 0);
	return 0;
}

int cutpurseEffect(int currentPlayer, int handPos, struct gameState *state, int* bonus) {
	int i, j, k;

	playedCard(handPos, NULL, NULL, state);

	*bonus += 2;
	for (i = 0; i < state->numPlayers; i++) {
		if (i != currentPlayer) {
			for (j = 0; j < state->handCount[i]; j++) {
				if (state->hand[i][j] == copper) {
					discardCard(j, i, state, 0);
				}
				if (j == state->handCount[i]) {
					for (k = 0; k < state->handCount[i]; k++) {
						if (DEBUG)
							printf("Player %d reveals card number %d\n", i, state->hand[i][k]);
					}
					break;
				}
			}// end looping through hand            
		}  //end if not current player        
	}// end for             

	//discard played card from hand
	endPlayed(state, 0);

	return 0;
}

int councilRoomEffect(int currentPlayer, int handPos, struct gameState *state) {
	int i;
	playedCard(handPos, NULL, NULL, state);
	//+4 Cards
	for (i = 0; i < 4; i++) {
		drawCard(currentPlayer, state);
	}

	//+1 Buy
	state->numBuys++;

	//Each other player draws a card
	for (i = 0; i < state->numPlayers; i++) {
		if (i != currentPlayer) {
			drawCard(i, state);
		}
	}

	//put played card in played card pile
	endPlayed(state, 0);

	return 0;
}


int cardEffect(int card, int choice1, int choice2, int choice3, struct gameState *state, int handPos, int *bonus)
{
	int i;
	int j;
	int index;
	int currentPlayer = whoseTurn(state);
	int nextPlayer = currentPlayer + 1;

	int tributeRevealedCards[2] = { -1, -1 };
	int temphand[MAX_HAND];// moved above the if statement
	int drawntreasure = 0;
	int cardDrawn;
	int z = 0;// this is the counter for the temp hand
	if (nextPlayer > (state->numPlayers - 1)) {
		nextPlayer = 0;
	}


	//uses switch to select card and perform actions
	switch (card)
	{
	case adventurer:
		playedCard(handPos, NULL, NULL, state);
		while (drawntreasure < 2) {
			if (drawCard(currentPlayer, state) == -1)
				break;
			cardDrawn = state->hand[currentPlayer][state->handCount[currentPlayer] - 1];//top card of hand is most recently drawn card.
			if (cardDrawn == copper || cardDrawn == silver || cardDrawn == gold)
				drawntreasure++;
			else {
				temphand[z] = cardDrawn;
				state->hand[currentPlayer][state->handCount[currentPlayer] - 1] = -1;
				state->handCount[currentPlayer]--; //this should just remove the top card (the most recently drawn one).
				z++;
			}
		}
		while (z > 0) {
			state->discard[currentPlayer][state->discardCount[currentPlayer]++] = temphand[z - 1]; // discard all cards in play that have been drawn
			z--;
		}
		endPlayed(state, 0);
		return 0;

	case council_room:
		return councilRoomEffect(currentPlayer, handPos, state);

	case feast:
		if (choice1 < curse || choice1 > treasure_map)
			return -1;

		if (supplyCount(choice1, state) <= 0) {
			if (DEBUG)
				printf("None of that card left, sorry!\n");

			if (DEBUG) {
				printf("Cards Left: %d\n", supplyCount(choice1, state));
			}
			return -1;
		}
		else if (5 < getCost(choice1)) {
			if (DEBUG) {
				printf("That card is too expensive!\n");
				printf("Coins: %d < %d\n", state->coins, getCost(choice1));
			}
			return -1;
		}	

		playedCard(handPos, NULL, NULL, state);

		if (DEBUG) {
			printf("Deck Count: %d\n", state->handCount[currentPlayer] + state->deckCount[currentPlayer] + state->discardCount[currentPlayer]);
		}

		gainCard(choice1, state, 0, currentPlayer);//Gain the card

		if (DEBUG) {
			printf("Deck Count: %d\n", state->handCount[currentPlayer] + state->deckCount[currentPlayer] + state->discardCount[currentPlayer]);
		}

		//trash feast
		endPlayed(state, 1);

		return 0;

	case gardens:
		return -1;

	case mine:
		if (choice1 >= state->handCount[currentPlayer] || choice1 < 0 || choice1 == handPos)
			return -1;

		j = state->hand[currentPlayer][choice1];  //store card we will trash

		if (state->hand[currentPlayer][choice1] < copper || state->hand[currentPlayer][choice1] > gold)
		{
			return -1;
		}

		if (choice2 > gold || choice2 < copper)
		{
			return -1;
		}

		if ((getCost(state->hand[currentPlayer][choice1]) + 3) > getCost(choice2))
		{
			return -1;
		}

		playedCard(handPos, &choice1, NULL, state);

		//trash old treasure
		discardCard(choice1, currentPlayer, state, 1);

		//gain new treasure
		gainCard(choice2, state, 2, currentPlayer);

		endPlayed(state, 0);

		return 0;

	case remodel:
		if (choice1 >= state->handCount[currentPlayer] || choice1 < 0 || choice1 == handPos)
			return -1;

		if (choice2 < curse || choice2 > treasure_map)
			return -1;

		if ((getCost(state->hand[currentPlayer][choice1]) + 2) > getCost(choice2))
		{
			return -1;
		}

		playedCard(handPos, &choice1, NULL, state);

		//trash choice
		discardCard(choice1, currentPlayer, state, 1);

		//gain new card
		gainCard(choice2, state, 0, currentPlayer);

		endPlayed(state, 0);
		return 0;

	case smithy:
		return smithyEffect(currentPlayer, handPos, state);

	case village:
		return villageEffect(currentPlayer, handPos, state);

	case baron:
		if (!(choice1 == 1 || choice1 == 2))
			return -1;

		if (choice1 == 1) {//Boolean true or going to discard an estate
			int p = 0;//Iterator for hand!
			int card_not_discarded = 1;//Flag for discard set!
			while (card_not_discarded) {
				if (p >= state->handCount[currentPlayer]) {
					if (DEBUG) {
						printf("No estate cards in your hand, invalid choice\n");						
					}
					return -1;
				}
				else if (state->hand[currentPlayer][p] == estate) {//Found an estate card!
					playedCard(handPos, &p, NULL, state);
					*bonus += 4;//Add 4 coins to the amount of coins
					discardCard(p, currentPlayer, state, 0);
					card_not_discarded = 0;//Exit the loop
				}
				else {
					p++;//Next card
				}
			}
		}

		else {
			playedCard(handPos, NULL, NULL, state);
			gainCard(estate, state, 0, currentPlayer);//Gain an estate	
		}
		state->numBuys++;//Increase buys by 1!
		endPlayed(state, 0);
		return 0;

	case great_hall:
		return greatHallEffect(currentPlayer, handPos, state);

	case minion:
		if (!(choice1 == 1 || choice1 == 2))
			return -1;

		playedCard(handPos, NULL, NULL, state);

		//+1 action
		state->numActions++;

		if (choice1 == 1)      //+2 coins
		{
			*bonus += 2;
		}

		else if (choice1 == 2)     //discard hand, redraw 4, other players with 5+ cards discard hand and draw 4
		{
			//discard hand
			while (numHandCards(state) > 0)
			{
				discardCard(0, currentPlayer, state, 0);
			}

			//draw 4
			for (i = 0; i < 4; i++)
			{
				drawCard(currentPlayer, state);
			}

			//other players discard hand and redraw if hand size > 4
			for (i = 0; i < state->numPlayers; i++)
			{
				if (i != currentPlayer)
				{
					if (state->handCount[i] > 4)
					{
						//discard hand
						while (state->handCount[i] > 0)
						{
							discardCard(0, i, state, 0);
						}

						//draw 4
						for (j = 0; j < 4; j++)
						{
							drawCard(i, state);
						}
					}
				}
			}

		}
		endPlayed(state, 0);
		return 0;

	case steward:
		if (!(choice1 == 1 || choice1 == 2 || choice1 == 3))
			return -1;
		if (choice1 == 3 && ((choice2 >= state->handCount[currentPlayer] || choice2 < 0) || (choice3 >= state->handCount[currentPlayer] || choice3 < 0) || choice2 == choice3 || (choice2 == handPos || choice3 == handPos)))
			return -1;

		if (choice1 == 1)
		{
			playedCard(handPos, NULL, NULL, state);
			//+2 cards
			drawCard(currentPlayer, state);
			drawCard(currentPlayer, state);
		}
		else if (choice1 == 2)
		{
			//+2 coins
			playedCard(handPos, NULL, NULL, state);
			*bonus += 2;
		}
		else
		{
			playedCard(handPos, &choice2, &choice3, state);
			//trash 2 cards in hand
			if (choice2 < choice3) {
				int tmp = choice2;
				choice2 = choice3;
				choice3 = tmp;
			}

			//discard order matters, must discard max to min for correct effect
			discardCard(choice2, currentPlayer, state, 1);
			discardCard(choice3, currentPlayer, state, 1);			
		}
		endPlayed(state, 0);
		return 0;

	case tribute:
		playedCard(handPos, NULL, NULL, state);
		if ((state->discardCount[nextPlayer] + state->deckCount[nextPlayer]) <= 1) {
			if (state->deckCount[nextPlayer] > 0) {
				tributeRevealedCards[0] = state->deck[nextPlayer][state->deckCount[nextPlayer] - 1];
				state->deckCount[nextPlayer]--;
				state->discard[nextPlayer][state->discardCount[nextPlayer]] = tributeRevealedCards[0];
				state->discardCount[nextPlayer]++;
			}
			else if (state->discardCount[nextPlayer] > 0) {
				tributeRevealedCards[0] = state->discard[nextPlayer][state->discardCount[nextPlayer] - 1];
			}
			else {
				//No Card to Reveal
				if (DEBUG) {
					printf("No cards to reveal\n");
				}
				endPlayed(state, 0);
				return 0;
			}
		}

		else {
			if (state->deckCount[nextPlayer] == 0) {
				j = state->discardCount[nextPlayer];
				for (i = 0; i < j; i++) {
					state->deck[nextPlayer][i] = state->discard[nextPlayer][i];//Move to deck
					state->deckCount[nextPlayer]++;
					state->discard[nextPlayer][i] = -1;
					state->discardCount[nextPlayer]--;
				}

				shuffle(nextPlayer, state);//Shuffle the deck
			}
			tributeRevealedCards[0] = state->deck[nextPlayer][state->deckCount[nextPlayer] - 1];
 /* MUTANT (rep_const) */			state->deck[nextPlayer][state->deckCount[nextPlayer] - -1] = -1;
			state->deckCount[nextPlayer]--;

			if (state->deckCount[nextPlayer] == 0) {
				j = state->discardCount[nextPlayer];
				for (i = 0; i < j; i++) {
					state->deck[nextPlayer][i] = state->discard[nextPlayer][i];//Move to deck
					state->deckCount[nextPlayer]++;
					state->discard[nextPlayer][i] = -1;
					state->discardCount[nextPlayer]--;
				}

				shuffle(nextPlayer, state);//Shuffle the deck
			}

			state->discard[nextPlayer][state->discardCount[nextPlayer]] = tributeRevealedCards[0];
			state->discardCount[nextPlayer]++;

			tributeRevealedCards[1] = state->deck[nextPlayer][state->deckCount[nextPlayer] - 1];
			state->deck[nextPlayer][state->deckCount[nextPlayer] - 1] = -1;
			state->deckCount[nextPlayer]--;
			state->discard[nextPlayer][state->discardCount[nextPlayer]] = tributeRevealedCards[1];
			state->discardCount[nextPlayer]++;
		}

		if (tributeRevealedCards[0] == tributeRevealedCards[1]) {//If we have a duplicate card, just drop one 
			tributeRevealedCards[1] = -1;
		}

		for (i = 0; i < 2; i++) {
			if (tributeRevealedCards[i] == copper || tributeRevealedCards[i] == silver || tributeRevealedCards[i] == gold) {//Treasure cards
				*bonus += 2;
			}

			if (tributeRevealedCards[i] == estate || tributeRevealedCards[i] == duchy || tributeRevealedCards[i] == province || tributeRevealedCards[i] == gardens || tributeRevealedCards[i] == great_hall) {//Victory Card Found
				drawCard(currentPlayer, state);
				drawCard(currentPlayer, state);
			}
			if (tributeRevealedCards[i] >= adventurer && tributeRevealedCards[i] <= treasure_map){//Action Card
				state->numActions = state->numActions + 2;
			}
		}

		endPlayed(state, 0);

		return 0;

	case ambassador:
		j = 0;        //used to check if player has enough cards to discard

		if (choice2 > 2 || choice2 < 0)
		{
			return -1;
		}

		if (choice1 == handPos || choice1 >= numHandCards(state) || choice1 < 0)
		{
			return -1;
		}

		for (i = 0; i < state->handCount[currentPlayer]; i++)
		{
			if (i != handPos && i == state->hand[currentPlayer][choice1])
			{
				j++;
			}
		}
		if (j < choice2)
		{
			return -1;
		}

		playedCard(handPos, &choice1, NULL, state);

		if (DEBUG)
			printf("Player %d reveals card number: %d\n", currentPlayer, state->hand[currentPlayer][choice1]);

		//increase supply count for choosen card by amount being discarded
		state->supplyCount[state->hand[currentPlayer][choice1]] += choice2;

		//each other player gains a copy of revealed card
		for (i = 0; i < state->numPlayers; i++)
		{
			if (i != currentPlayer)
			{
				gainCard(state->hand[currentPlayer][choice1], state, 0, i);
			}
		}

		//trash copies of cards returned to supply
		for (j = 0; j < choice2; j++)
		{
			for (i = state->handCount[currentPlayer] - 1; i >= 0; i--)
			{
				if (state->hand[currentPlayer][i] == state->hand[currentPlayer][choice1])
				{
					discardCard(i, currentPlayer, state, 1);
					break;
				}
			}
		}

		endPlayed(state, 0);

		return 0;

	case cutpurse:
		return cutpurseEffect(currentPlayer, handPos, state, bonus);

	case embargo:
		if (choice1 < curse || choice1 > treasure_map)
			return -1;
		//see if selected pile is in play
		if (state->supplyCount[choice1] == -1)
		{
			return -1;
		}

		playedCard(handPos, NULL, NULL, state);

		//+2 Coins
		*bonus += 2;

		//add embargo token to selected supply pile
		state->embargoTokens[choice1]++;

		//trash card
		endPlayed(state, 1);
		return 0;

	case outpost:
		if (state->outpostTurn == 1)
			return -1;
		
		playedCard(handPos, NULL, NULL, state);
		//set outpost flag
		state->outpostPlayed = 1;

		//we actually don't call endPlayed() here on purpose
		return 0;

	case salvager:
		if (choice1 >= state->handCount[currentPlayer] || choice1 < 0 || choice1 == handPos)
			return -1;

		playedCard(handPos, &choice1, NULL, state);

		//+1 buy
		state->numBuys++;
		
		//gain coins equal to trashed card
		*bonus += getCost(handCard(choice1, state));
		
		//trash card
		discardCard(choice1, currentPlayer, state, 1);
		
		endPlayed(state, 0);
		return 0;

	case sea_hag:
		playedCard(handPos, NULL, NULL, state);
		for (i = 0; i < state->numPlayers; i++) {
			if (i != currentPlayer) {
				if (state->deckCount[i] + state->discardCount[i] > 0) {
					if (state->deckCount[i] == 0) {
						j = state->discardCount[i];
						for (index = 0; index < j; index++) {
							state->deck[i][index] = state->discard[i][index];//Move to deck
							state->deckCount[i]++;
							state->discard[i][index] = -1;
							state->discardCount[i]--;
						}

						shuffle(i, state);//Shuffle the deck
					}
					state->discard[i][state->discardCount[i]] = state->deck[i][state->deckCount[i] - 1];
					state->discardCount[i]++;
					//conveniently, this happens to add it to the top of the deck
					gainCard(curse, state, 1, i);
				}
				else { //literally no cards in their deck or discard, so they just get a curse in their deck
					gainCard(curse, state, 1, i);
				}				
			}
		}
		endPlayed(state, 0);
		return 0;

	case treasure_map:
		//search hand for another treasure_map
		index = -1;
		for (i = 0; i < state->handCount[currentPlayer]; i++)
		{
			if (state->hand[currentPlayer][i] == treasure_map && i != handPos)
			{
				index = i;
				break;
			}
		}
		if (index > -1){
			playedCard(handPos, &index, NULL, state);
			//trash other treasure_map
			discardCard(index, currentPlayer, state, 1);

			//gain 4 Gold cards
			for (i = 0; i < 4; i++)
			{
				gainCard(gold, state, 1, currentPlayer);
			}
		}
		else {
			return -1;
		}
		endPlayed(state, 1);
		return 0;
		
	}

	return -1;
}

int discardCard(int handPos, int currentPlayer, struct gameState *state, int trashFlag)
{

	//if card is not trashed, added to discard
	if (trashFlag < 1)
	{
		//add card to discard pile
		state->discard[currentPlayer][state->discardCount[currentPlayer]] = state->hand[currentPlayer][handPos];
		state->discardCount[currentPlayer]++;
	}

	//set played card to -1
	state->hand[currentPlayer][handPos] = -1;

	//remove card from player's hand
	if (handPos == (state->handCount[currentPlayer] - 1))   //last card in hand array is played
	{
		//reduce number of cards in hand
		state->handCount[currentPlayer]--;
	}
	else if (state->handCount[currentPlayer] == 1) //only one card in hand
	{
		//reduce number of cards in hand
		state->handCount[currentPlayer]--;
	}
	else
	{
		//replace discarded card with last card in hand
		state->hand[currentPlayer][handPos] = state->hand[currentPlayer][(state->handCount[currentPlayer] - 1)];
		//set last card to -1
		state->hand[currentPlayer][state->handCount[currentPlayer] - 1] = -1;
		//reduce number of cards in hand
		state->handCount[currentPlayer]--;
	}

	return 0;
}

int gainCard(int supplyPos, struct gameState *state, int toFlag, int player)
{
	//Note: supplyPos is enum of choosen card

	//check if supply pile is empty (0) or card is not used in game (-1)
	if (supplyCount(supplyPos, state) < 1)
	{
		return -1;
	}

	//added card for [whoseTurn] current player:
	// toFlag = 0 : add to discard
	// toFlag = 1 : add to deck
	// toFlag = 2 : add to hand

	if (toFlag == 1)
	{
		state->deck[player][state->deckCount[player]] = supplyPos;
		state->deckCount[player]++;
	}
	else if (toFlag == 2)
	{
		state->hand[player][state->handCount[player]] = supplyPos;
		state->handCount[player]++;
	}
	else
	{
		state->discard[player][state->discardCount[player]] = supplyPos;
		state->discardCount[player]++;
	}

	//decrease number in supply pile
	state->supplyCount[supplyPos]--;

	return 0;
}

int updateCoins(int player, struct gameState *state, int bonus)
{
	int i;
	static int cumulative = 0;
	static int lastPlayer = -1;

	if (lastPlayer == player) {
		cumulative += bonus;
	}
	else {
		cumulative = bonus;
		lastPlayer = player;
	}

	//reset coin count
	state->coins = 0;

	//add coins for each Treasure card in player's hand
	for (i = 0; i < state->handCount[player]; i++)
	{
		if (state->hand[player][i] == copper)
		{
			state->coins += 1;
		}
		else if (state->hand[player][i] == silver)
		{
			state->coins += 2;
		}
		else if (state->hand[player][i] == gold)
		{
			state->coins += 3;
		}
	}

	//add bonus
	state->coins += cumulative;

	return 0;
}


//end of dominion.c

