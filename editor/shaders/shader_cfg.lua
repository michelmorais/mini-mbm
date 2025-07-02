
--Shader CFG as LUA   -> easy :)
--Use mbm.include("shader_cfg.lua") to add to your game 
--mbm.addShader doesn't verify if the shader is right.

local function addShader(tShader)
    if not mbm.existShader(tShader.name) and not mbm.addShader(tShader) then
        print("Error on add shader:",tShader.name)
    end
end

local tShaderGradient_horizontal = 
{   name = 'gradient_horizontal.ps',
    code = [[
    precision mediump float;
    uniform sampler2D sample0;
    uniform vec4 right;
    uniform vec4 left;
    varying vec2 vTexCoord;

    void main()
    {
        vec4 c1 = mix(left,right,vTexCoord.x);
        c1.a = min(texture2D(sample0, vTexCoord).a,c1.a);
        gl_FragColor = c1;
    }
    ]],
    var = {right = {0.5,0.5,0.5,0.5}, left = {1,1,1,1}},
    min = {right = {0.0,0.0,0.0,0.0}, left = {0,0,0,0}},
    max = {right = {1.0,1.0,1.0,1.0}, left = {1,1,1,1}}
}
addShader(tShaderGradient_horizontal)

local tShaderGradient_vertical = 
{   name = 'gradient_vertical.ps',
    code = [[
    precision mediump float;
    uniform sampler2D sample0;
    uniform vec4 top;
    uniform vec4 down;
    varying vec2 vTexCoord;

    void main()
    {
        vec4 c1 = mix(down,top,vTexCoord.y);
        c1.a = min(texture2D(sample0, vTexCoord).a,c1.a);
        gl_FragColor = c1;
    }
    ]],
    var = {top = {0.5,0.5,0.5,0.5}, down = {1,1,1,1}},
    min = {top = {0.0,0.0,0.0,0.0}, down = {0,0,0,0}},
    max = {top = {1.0,1.0,1.0,1.0}, down = {1,1,1,1}}
}
addShader(tShaderGradient_vertical)


local tShaderLife = 
{   
    name = 'life.ps',
    code = [[
    precision mediump float;
    uniform sampler2D sample0;
    uniform vec4 right;
    uniform vec4 left;
    uniform float my_life;
    varying vec2 vTexCoord;

    void main()
    {
        vec4 c1 = mix(left,right,vTexCoord.x);
        if((vTexCoord.x) > my_life)
            c1.a = 0.0;
        else
            c1.a = min(texture2D(sample0, vTexCoord).a,c1.a);
        gl_FragColor = c1;
    }
    ]],
    var = {right = {0.5,0.5,0.5,0.5}, left = {1,1,1,1}, my_life = {1}},
    min = {right = {0.0,0.0,0.0,0.0}, left = {0,0,0,0}, my_life = {0}},
    max = {right = {1.0,1.0,1.0,1.0}, left = {1,1,1,1}, my_life = {1}}
}
addShader(tShaderLife)

local tShaderParalax = 
{   
    name = 'paralax.ps',
    code = [[
    precision mediump float;
    uniform sampler2D sample0;
    uniform vec2 position;
    uniform vec2 scale;
    varying vec2 vTexCoord;

    void main()
    {
        vec2 newposition = (vTexCoord + position) * scale;
        if (newposition.x < 0.0 || newposition.x > 1.0 || 
            newposition.y < 0.0 || newposition.y > 1.0 )
        {
            vec4 color = vec4(0.0,0.0,0.0,0.0);
            gl_FragColor = color;
        }   
        else
        {
            vec4 color = texture2D(sample0, newposition);
            gl_FragColor = color;
        }
    }
    ]],
    var = {position = { 0.0, 0.0},  scale={1.0,1.0}},
    min = {position = {-10.0,-10.0},scale={0.01,0.01}},
    max = {position = { 10.0, 10.0},scale={20.0,20.0}},
}
addShader(tShaderParalax)

local tVerticalGradient = 
{   name = 'vertical gradient.ps',
    code = [[
    
    precision mediump float;
    uniform sampler2D sample0;
    varying vec2 vTexCoord;
    uniform vec4 top;
    uniform vec4 down;
    
    void main( )
    {
        vec4 c1 = mix(top,down,vTexCoord.y);
        c1.a = min(texture2D(sample0, vTexCoord).a,c1.a);
        gl_FragColor = c1;
    }

    ]],
    var = {down = {1.0,1.0,1.0,1.0}, top = {0.0,0.0,0.0,1.0}, },
    min = {down = {0.0,0.0,0.0,0.0}, top = {0.0,0.0,0.0,0.0}, },
    max = {down = {1.0,1.0,1.0,1.0}, top = {1.0,1.0,1.0,1.0}, }
}

addShader(tVerticalGradient)


local tPie = 
{   name = 'pie.ps',
    code = [[
    
    precision mediump float;
    uniform sampler2D sample0;
    varying vec2 vTexCoord;
    uniform float percent;
	uniform float angle;
	uniform float clockwise;
    
    void main( )
    {
		float x = vTexCoord.x - 0.5;
		float y = vTexCoord.y - 0.5;
		x = x * cos(angle) - y * sin(angle);
		y = x * sin(angle) + y * cos(angle);
		
		float a = atan(y,x);
		
		if ((clockwise >= 0.5 && a > percent) || (clockwise < 0.5 && a < percent))
		{
			gl_FragColor = texture2D(sample0, vTexCoord);
		}
		else
		{
			discard;
		}
    }

    ]],
    var = {percent = {0}      , angle = {0}      , clockwise = {1.0}},
    min = {percent = {-3.1415}, angle = {-3.1415}, clockwise = {0.0}},
    max = {percent = {3.1415} , angle = {3.1415} , clockwise = {1.0}}
}

addShader(tPie)
