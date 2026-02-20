#include "BTTask_SelectPatrolPoint.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyParent.h"

//Comment the below header in shipping build
#include "DrawDebugHelpers.h"

EBTNodeResult::Type UBTTask_SelectPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIControllerRef = OwnerComp.GetAIOwner();
	if (!AIControllerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("AIControllerRef is NULL"));
		return EBTNodeResult::Failed;
	}
	APawn* OwningPawn = AIControllerRef->GetPawn();
	if (!OwningPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("APawn {Enemy Pawn} is NULL"));
		return EBTNodeResult::Failed;
	}
	
	AEnemyParent* OwningEnemyCharacter = Cast<AEnemyParent>(OwningPawn);
	if (!OwningEnemyCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("Cast to AAEnemy Failed"));
		return EBTNodeResult::Failed;
	}
	
	TObjectPtr<UBlackboardComponent> BlackboardComponent = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("Blackboard Component is NULL"));
		return EBTNodeResult::Failed;
	}
	DrawDebugSphere(OwningPawn->GetWorld(), OwningEnemyCharacter->PatrolCenter, PointRadius, 12, FColor::White, true, -1.0f);
	if (UNavigationSystemV1* current = UNavigationSystemV1::GetCurrent(OwningPawn->GetWorld()))
	{
		FNavLocation ResultLocation;
		if (current->GetRandomReachablePointInRadius(OwningEnemyCharacter->PatrolCenter, PointRadius, ResultLocation) == true)
		{
			BlackboardComponent->SetValueAsVector("PatrolLocation", ResultLocation.Location);
			DrawDebugSphere(OwningPawn->GetWorld(), ResultLocation.Location, 2.f, 12, FColor::Red, false, 5.f);
			return EBTNodeResult::Succeeded;
		}
	}
	return EBTNodeResult::Failed;
}
