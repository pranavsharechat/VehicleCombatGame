// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleUE5Pawn.h"
#include "VehicleUE5WheelFront.h"
#include "VehicleUE5WheelRear.h"
#include "VehicleUE5Hud.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"


const FName AVehicleUE5Pawn::LookUpBinding("LookUp");
const FName AVehicleUE5Pawn::LookRightBinding("LookRight");
const FName AVehicleUE5Pawn::EngineAudioRPM("RPM");

#define LOCTEXT_NAMESPACE "VehiclePawn"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

AVehicleUE5Pawn::AVehicleUE5Pawn()
{
	// Car mesh
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/Vehicles/Vehicle/Vehicle_SkelMesh.Vehicle_SkelMesh"));
	GetMesh()->SetSkeletalMesh(CarMesh.Object);
	GetMesh()->SetSimulatePhysics(true);
	
	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/Vehicles/Vehicle/VehicleAnimationBlueprint"));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);

	// Setup friction materials
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> SlipperyMat(TEXT("/Game/Vehicles/PhysicsMaterials/Slippery.Slippery"));
	SlipperyMaterial = SlipperyMat.Object;
		
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> NonSlipperyMat(TEXT("/Game/Vehicles/PhysicsMaterials/NonSlippery.NonSlippery"));
	NonSlipperyMaterial = NonSlipperyMat.Object;

	UChaosWheeledVehicleMovementComponent* VehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

	// Wheels/Tyres
	// Setup the wheels
	VehicleMovement->WheelSetups.SetNum(4);
	{
		VehicleMovement->WheelSetups[0].WheelClass = UVehicleUE5WheelFront::StaticClass();
		VehicleMovement->WheelSetups[0].BoneName = FName("PhysWheel_FL");
		VehicleMovement->WheelSetups[0].AdditionalOffset = FVector(0.f, -8.f, 0.f);

		VehicleMovement->WheelSetups[1].WheelClass = UVehicleUE5WheelFront::StaticClass();
		VehicleMovement->WheelSetups[1].BoneName = FName("PhysWheel_FR");
		VehicleMovement->WheelSetups[1].AdditionalOffset = FVector(0.f, 8.f, 0.f);

		VehicleMovement->WheelSetups[2].WheelClass = UVehicleUE5WheelRear::StaticClass();
		VehicleMovement->WheelSetups[2].BoneName = FName("PhysWheel_BL");
		VehicleMovement->WheelSetups[2].AdditionalOffset = FVector(0.f, -8.f, 0.f);

		VehicleMovement->WheelSetups[3].WheelClass = UVehicleUE5WheelRear::StaticClass();
		VehicleMovement->WheelSetups[3].BoneName = FName("PhysWheel_BR");
		VehicleMovement->WheelSetups[3].AdditionalOffset = FVector(0.f, 8.f, 0.f);
	}

	// Engine 
	// Torque setup
	VehicleMovement->EngineSetup.MaxRPM = 5700.0f;
	VehicleMovement->EngineSetup.MaxTorque = 500.0f;
	VehicleMovement->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
	VehicleMovement->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
	VehicleMovement->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(1890.0f, 500.0f);
	VehicleMovement->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(5730.0f, 400.0f);
 
	// This works because the AxleType has been setup on the wheels
	VehicleMovement->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;

	// Adjust the steering 
	VehicleMovement->SteeringSetup.SteeringCurve.GetRichCurve()->Reset();
	VehicleMovement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(0.0f, 1.0f);
	VehicleMovement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(40.0f, 0.7f);
	VehicleMovement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(120.0f, 0.6f);
			
	// Drive the front wheels a little more than the rear
	VehicleMovement->DifferentialSetup.FrontRearSplit = 0.65;

	// Automatic gearbox
	VehicleMovement->TransmissionSetup.bUseAutomaticGears = true;
	VehicleMovement->TransmissionSetup.bUseAutoReverse = true;
	VehicleMovement->TransmissionSetup.GearChangeTime = 0.15f;

	// Physics settings
	// Adjust the center of mass - the buggy is quite low
	UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(VehicleMovement->UpdatedComponent);
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->BodyInstance.COMNudge = FVector(8.0f, 0.0f, -15.0f);
	}

	// Set the inertia scale. This controls how the mass of the vehicle is distributed.
	VehicleMovement->InertiaTensorScale = FVector(1.0f, 1.333f, 1.2f);

	RotatingAnchorSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RotatingSceneComponent"));
	RotatingAnchorSceneComponent->SetupAttachment(RootComponent);

	// Create a spring arm component for our chase camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));
	SpringArm->SetWorldRotation(FRotator(-20.0f, 0.0f, 0.0f));
	SpringArm->SetupAttachment(RotatingAnchorSceneComponent);
	SpringArm->TargetArmLength = 400.0f;
	SpringArm->bEnableCameraLag = true;
	SpringArm->bEnableCameraRotationLag = false;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = true;

	// Create the chase camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetRelativeLocation(FVector(-125.0, 0.0f, 0.0f));
	Camera->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	// Create In-Car camera component 
	InternalCameraOrigin = FVector(-34.0f, -10.0f, 50.0f);
	InternalCameraBase = CreateDefaultSubobject<USceneComponent>(TEXT("InternalCameraBase"));
	InternalCameraBase->SetRelativeLocation(InternalCameraOrigin);
	InternalCameraBase->SetupAttachment(GetMesh());

	InternalCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InternalCamera"));
	InternalCamera->bUsePawnControlRotation = false;
	InternalCamera->FieldOfView = 90.f;
	InternalCamera->SetupAttachment(InternalCameraBase);

	// In car HUD
	// Create text render component for in car speed display
	InCarSpeed = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarSpeed"));
	InCarSpeed->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarSpeed->SetRelativeLocation(FVector(35.0f, -6.0f, 20.0f));
	InCarSpeed->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarSpeed->SetupAttachment(GetMesh());

	// Create text render component for in car gear display
	InCarGear = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
	InCarGear->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarGear->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
	InCarGear->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarGear->SetupAttachment(GetMesh());
	

	// Setup the audio component and allocate it a sound cue
	static ConstructorHelpers::FObjectFinder<USoundCue> SoundCue(TEXT("/Game/Vehicles/Sound/Engine_Loop_Cue.Engine_Loop_Cue"));
	EngineSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineSound"));
	EngineSoundComponent->SetSound(SoundCue.Object);
	EngineSoundComponent->SetupAttachment(GetMesh());

	// Colors for the in-car gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	bIsLowFriction = false;
	bInReverseGear = false;
}

void AVehicleUE5Pawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AVehicleUE5Pawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AVehicleUE5Pawn::MoveRight);
	PlayerInputComponent->BindAxis(LookUpBinding);
	PlayerInputComponent->BindAxis(LookRightBinding);

	PlayerInputComponent->BindAction("Handbrake", IE_Pressed, this, &AVehicleUE5Pawn::OnHandbrakePressed);
	PlayerInputComponent->BindAction("Handbrake", IE_Released, this, &AVehicleUE5Pawn::OnHandbrakeReleased);
	PlayerInputComponent->BindAction("SwitchCamera", IE_Pressed, this, &AVehicleUE5Pawn::OnToggleCamera);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AVehicleUE5Pawn::OnResetVR); 
}

void AVehicleUE5Pawn::MoveForward(float Val)
{
	if (Val > 0)
	{
		GetVehicleMovementComponent()->SetThrottleInput(Val);
		GetVehicleMovementComponent()->SetBrakeInput(0.f);
	}
	else
	{
		GetVehicleMovementComponent()->SetThrottleInput(0.f);
		GetVehicleMovementComponent()->SetBrakeInput(-Val);
	}
}

void AVehicleUE5Pawn::MoveRight(float Val)
{
	GetVehicleMovementComponent()->SetSteeringInput(Val);
}

void AVehicleUE5Pawn::OnHandbrakePressed()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
}

void AVehicleUE5Pawn::OnHandbrakeReleased()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
}

void AVehicleUE5Pawn::OnToggleCamera()
{
	EnableIncarView(!bInCarCameraActive);
}

void AVehicleUE5Pawn::EnableIncarView(const bool bState)
{
	if (bState != bInCarCameraActive)
	{
		bInCarCameraActive = bState;
		
		if (bState == true)
		{
			OnResetVR();
			Camera->Deactivate();
			InternalCamera->Activate();
		}
		else
		{
			InternalCamera->Deactivate();
			Camera->Activate();
		}
		
		InCarSpeed->SetVisibility(bInCarCameraActive);
		InCarGear->SetVisibility(bInCarCameraActive);
	}
}

void AVehicleUE5Pawn::Tick(float Delta)
{
	Super::Tick(Delta);


	RotatingAnchorSceneComponent->AddLocalRotation(FRotator(0, 1, 0), false, nullptr, ETeleportType::None);
	// Setup the flag to say we are in reverse gear
	bInReverseGear = GetVehicleMovement()->GetCurrentGear() < 0;
	
	// Update phsyics material
	UpdatePhysicsMaterial();

	// Update the strings used in the hud (incar and onscreen)
	UpdateHUDStrings();

	// Set the string in the incar hud
	SetupInCarHUD();

	bool bHMDActive = false;
	if( bHMDActive == false )
	{
		if ( (InputComponent) && (bInCarCameraActive == true ))
		{
			FRotator HeadRotation = InternalCamera->GetRelativeRotation();
			HeadRotation.Pitch += InputComponent->GetAxisValue(LookUpBinding);
			HeadRotation.Yaw += InputComponent->GetAxisValue(LookRightBinding);
			InternalCamera->SetRelativeRotation(HeadRotation);
		}
	}	

	// Pass the engine RPM to the sound component
	UChaosWheeledVehicleMovementComponent* WheeledVehicle = static_cast<UChaosWheeledVehicleMovementComponent*>(GetVehicleMovement());
	float RPMToAudioScale = 2500.0f / WheeledVehicle->GetEngineMaxRotationSpeed();
	EngineSoundComponent->SetFloatParameter(EngineAudioRPM, WheeledVehicle->GetEngineRotationSpeed()*RPMToAudioScale);
}

void AVehicleUE5Pawn::BeginPlay()
{
	Super::BeginPlay();

	bool bWantInCar = false;
	// First disable both speed/gear displays 
	bInCarCameraActive = false;
	InCarSpeed->SetVisibility(bInCarCameraActive);
	InCarGear->SetVisibility(bInCarCameraActive);

	EnableIncarView(bWantInCar);
	// Start an engine sound playing
	EngineSoundComponent->Play();

	DisableInput(GetLocalViewingPlayerController());
}

void AVehicleUE5Pawn::OnResetVR()
{
}

void AVehicleUE5Pawn::UpdateHUDStrings()
{
	float KPH = FMath::Abs(GetVehicleMovement()->GetForwardSpeed()) * 0.036f;
	int32 KPH_int = FMath::FloorToInt(KPH);
	int32 Gear = GetVehicleMovement()->GetCurrentGear();

	// Using FText because this is display text that should be localizable
	SpeedDisplayString = FText::Format(LOCTEXT("SpeedFormat", "{0} km/h"), FText::AsNumber(KPH_int));


	if (bInReverseGear == true)
	{
		GearDisplayString = FText(LOCTEXT("ReverseGear", "R"));
	}
	else
	{
		GearDisplayString = (Gear == 0) ? LOCTEXT("N", "N") : FText::AsNumber(Gear);
	}

}

void AVehicleUE5Pawn::SetupInCarHUD()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if ((PlayerController != nullptr) && (InCarSpeed != nullptr) && (InCarGear != nullptr))
	{
		// Setup the text render component strings
		InCarSpeed->SetText(SpeedDisplayString);
		InCarGear->SetText(GearDisplayString);
		
		if (bInReverseGear == false)
		{
			InCarGear->SetTextRenderColor(GearDisplayColor);
		}
		else
		{
			InCarGear->SetTextRenderColor(GearDisplayReverseColor);
		}
	}
}

void AVehicleUE5Pawn::UpdatePhysicsMaterial()
{
	if (GetActorUpVector().Z < 0)
	{
		if (bIsLowFriction == true)
		{
			GetMesh()->SetPhysMaterialOverride(NonSlipperyMaterial);
			bIsLowFriction = false;
		}
		else
		{
			GetMesh()->SetPhysMaterialOverride(SlipperyMaterial);
			bIsLowFriction = true;
		}
	}
}

#undef LOCTEXT_NAMESPACE

PRAGMA_ENABLE_DEPRECATION_WARNINGS