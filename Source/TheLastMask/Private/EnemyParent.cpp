#include "EnemyParent.h"

#include "EnemyAIController.h"

AEnemyParent::AEnemyParent()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AEnemyParent::BeginPlay()
{
	Super::BeginPlay();
	PatrolCenter = GetActorLocation();
	
}


