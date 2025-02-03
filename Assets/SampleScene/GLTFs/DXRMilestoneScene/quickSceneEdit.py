import yaml
import random

if __name__ == "__main__":
    with open('DXRMilestone.yaml', 'r') as f:
        data = yaml.load(f, Loader=yaml.SafeLoader)
    
    tallBoxCnt = 0
    sceneGraphDict = data['SceneGraph']
    for key, value in sceneGraphDict.items():
        if value['Type'] == 'StaticMesh':
            if 'AssetPath' in value and value['AssetPath'] == 'tallBox.gltf':
                # print(f"Key: {key}, Value: {value}")
                value['Position'][1] = -4.0 + random.random() - 0.5
                value['Material']['Albedo'][0] = random.random()
                value['Material']['Albedo'][1] = random.random()
                value['Material']['Albedo'][2] = random.random()
                tallBoxCnt += 1
    
    print(f"tallBox count: {tallBoxCnt}")

    with open('data.yaml', 'w') as file:
        yaml.dump(data, file)