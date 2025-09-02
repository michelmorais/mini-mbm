require "box2dLiquidFun"
 mbm.setColor(0.6,0.6,0.6)

 tPhysic = box2dLiquidFun:new()

 tShapeGround    = shape:new('2dw',0,-200)
 tShapeLeftWall  = shape:new('2dw',-200,0)
 tShapeRightWall = shape:new('2dw',200,0)
 tShapeUpWall    = shape:new('2dw',0,200)

 tShapeGround:create('rectangle',400,20)
 tShapeRightWall:create('rectangle',20,400)
 tShapeLeftWall:create('rectangle',20,400)
 tShapeUpWall:create('rectangle',400,20)

 tPhysic:addStaticBody(tShapeGround)
 tPhysic:addStaticBody(tShapeRightWall)
 tPhysic:addStaticBody(tShapeLeftWall)
 tPhysic:addStaticBody(tShapeUpWall)

 tFluid = tPhysic:createFluid(
     {   type   = 'rectangle',
         center = {x=0,y=0,z=0},
         width  = 200, height = 400,
     },
     {   texture     ='#FFF00F',
         color       = {r = 0.0, g = 0.0, b =1.0},
         radiusScale = 2.0,
         flags       = "water",
         groupFlags  = {"solidParticleGroup"},
     })