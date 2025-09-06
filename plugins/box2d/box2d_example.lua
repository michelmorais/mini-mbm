require "box2d"
function print_manifold_world(tWorldManifold)
    print('normal.x:',tWorldManifold.normal.x)
    print('normal.x:',tWorldManifold.normal.y)
    print('separations[1]:',tWorldManifold.separations[1])
    print('separations[2]:',tWorldManifold.separations[2])

    for i =1, #tWorldManifold.points do
        print('point:',i)
        print('\tx:',tWorldManifold.points[1].x)
        print('\ty:',tWorldManifold.points[1].y)
    end
    print('\n')
end

-- Callbacks
function onPreSolve(tMesh_a, tMesh_b, tManifold)
    local tWorldManifolds = tPhysic:getWorldManifolds(tMesh_a)
    for i =1, #tWorldManifolds do
        print('WorldManifold index ', i)
        print_manifold_world(tWorldManifolds[i])
    end
end

-- End callbacks

mbm.setColor(1,1,1) --set background color to white
tPhysic = box2d:new()

tShapeQuad       = shape:new('2dw',-10, 500)
tShapeCircle     = shape:new('2dw',50 , 300)
tShapeGround     = shape:new('2dw',0,-50)
tShapeBigCircle  = shape:new('2dw',200,800)

tShapeQuad.name      = 'rectangle'
tShapeCircle.name    = 'circle'
tShapeBigCircle.name = 'big circle'
tShapeGround.name    = 'ground'

tShapeQuad:create('rectangle',100,100)
tShapeCircle:create('circle',100,100)
tShapeBigCircle:create('circle',200,200)
tShapeGround:create('rectangle',1000,20)

tPhysic:addDynamicBody(tShapeQuad)
tPhysic:addDynamicBody(tShapeCircle)
tPhysic:addDynamicBody(tShapeBigCircle)
tPhysic:addStaticBody(tShapeGround)

tPhysic:setContactListener(nil,nil,onPreSolve,nil)

camera = mbm.getCamera('2d')
camera.y = 300