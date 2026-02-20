#include "EnemyAIController.h"



AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (BehaviorTree)
	{
		RunBehaviorTree(BehaviorTree);
	}
	else UE_LOG(LogTemp, Warning, TEXT("Behaviour Tree is a Null Pointer!"));
}


