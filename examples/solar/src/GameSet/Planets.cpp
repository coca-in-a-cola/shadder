#include "Planets.hpp"

enum MeshType { Cube, Sphere };

static std::shared_ptr<GameComponent> CreateBody(
	GameContext* ctx, const Vector3& position, const Vector3& scale, const Vector3& angularVelocity, float angularSpeed,
	MeshType meshType = MeshType::Cube
)
{
	auto object = std::make_shared<GameComponent>(ctx);
	object->position = position;

	auto body = std::make_shared<GameComponent>(ctx);
	body->scale = scale;
	body->SetAngularVelocity(angularVelocity);
	body->angularSpeed = angularSpeed;

	auto mesh = std::make_unique<Mesh>();
	if (meshType == MeshType::Cube) {
		mesh->CreateCube(ctx);
	} else {
		mesh->CreateSphere(ctx);
	}
	body->SetMesh(std::move(mesh));

	object->AddChild(body);

	return object;
}

static std::shared_ptr<GameComponent> CreateOrbit(GameContext* ctx, const Vector3& angularVelocity, float angularSpeed)
{
	auto orbit = std::make_shared<GameComponent>(ctx);
	orbit->SetAngularVelocity(angularVelocity);
	orbit->angularSpeed = angularSpeed;
	return orbit;
}

std::vector<std::shared_ptr<GameComponent>> CreatePlanetsGame(GameContext* ctx)
{
	std::vector<std::shared_ptr<GameComponent>> components;

	auto grid = std::make_shared<GameComponent>(ctx);

	auto gridMesh = std::make_unique<Mesh>();
	gridMesh->CreateGrid(ctx, 120.0f, 24, Colors::LightGray);
	grid->SetMesh(std::move(gridMesh));
	components.push_back(grid);

	auto sun = CreateBody(ctx, Vector3(0, 0, 0), Vector3(4, 4, 4), Vector3(0, 1, 0), 0.3f, MeshType::Sphere);
	components.push_back(sun);

	auto sunOrbit1 = CreateOrbit(ctx, Vector3(0, 1, 0), 1.0f);
	sun->AddChild(sunOrbit1);

	auto sunOrbit2 = CreateOrbit(ctx, Vector3(0, 1.0f, 1.5f), 5.0f);
	sun->AddChild(sunOrbit2);

	auto sunOrbit3 = CreateOrbit(ctx, Vector3(1, 1, 0), 0.2f);
	sun->AddChild(sunOrbit3);

	auto sunOrbit4 = CreateOrbit(ctx, Vector3(1, 1, 0), 0.1f);
	sun->AddChild(sunOrbit4);

	auto planet1 = CreateBody(ctx, Vector3(5, 0, 0), Vector3(1, 1, 1), Vector3(0.3f, 1, 0), 0.7f);
	sunOrbit1->AddChild(planet1);

	auto planet2 = CreateBody(ctx, Vector3(-10, 0, 3), Vector3(1, 1, 1), Vector3(0, 1, 0), 0.4f);
	sunOrbit1->AddChild(planet2);

	auto planet3 = CreateBody(ctx, Vector3(15, 0, 0), Vector3(1, 1, 1), Vector3(1, 1, 1), 2.0f);
	sunOrbit2->AddChild(planet3);

	auto planet4 = CreateBody(ctx, Vector3(-27, 0, 0), Vector3(2, 2, 2), Vector3(0, 1, 1), 0.2f);
	sunOrbit3->AddChild(planet4);

	auto planet4Orbit = CreateOrbit(ctx, Vector3(0, 1, 0), 1.0f);
	planet4->AddChild(planet4Orbit);

	auto planet4Moon1 = CreateBody(ctx, Vector3(0, 0, -5), Vector3(1, 1, 1), Vector3(0, 1, 0), 0.5f);
	planet4Orbit->AddChild(planet4Moon1);

	auto planet4Moon2 = CreateBody(ctx, Vector3(0, 0, 5), Vector3(0.5f, 0.5f, 0.5f), Vector3(0, 1, 0), 0.5f);
	planet4Orbit->AddChild(planet4Moon2);

	auto planet5 =
		CreateBody(ctx, Vector3(36, 0, 0), Vector3(2.5f, 2.5f, 2.5f), Vector3(0, 1, 0), 0.4f, MeshType::Sphere);
	sunOrbit3->AddChild(planet5);

	auto planet6 = CreateBody(ctx, Vector3(60, 0, 0), Vector3(3, 3, 3), Vector3(1, 0, 1), 0.1f);
	sunOrbit4->AddChild(planet6);

	auto planet6Orbit = CreateOrbit(ctx, Vector3(0, 1, 0), 1.0f);
	planet6->AddChild(planet6Orbit);

	auto planet6Moon = CreateBody(ctx, Vector3(0, 0, -6), Vector3(1, 1, 1), Vector3(0, 1, 0), 0.5f);
	planet6Orbit->AddChild(planet6Moon);

	auto planet6MoonOrbit = CreateOrbit(ctx, Vector3(1, 0, 0), 1.0f);
	planet6Moon->AddChild(planet6MoonOrbit);

	auto planet6MoonMoon = CreateBody(ctx, Vector3(0, -3, 0), Vector3(0.5, 0.5, 0.5), Vector3(1, 0, 0), 0.5f);
	planet6MoonOrbit->AddChild(planet6MoonMoon);

	return components;
}
