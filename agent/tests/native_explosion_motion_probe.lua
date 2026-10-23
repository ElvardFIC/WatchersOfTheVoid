-- Isolated native LAN fixture. Write "sourceKey targetKey watch" to blast.txt,
-- wait for both baselines, then change watch to fire. The host moves only the
-- gascan; the target starts on its original collision surface. Never use a real save.
local evidence=os.getenv('EXPLOSION_MOTION_PROBE_DIR') or os.getenv('ATV_PROBE_DIR')
if not evidence then return end
local role=os.getenv('VOTVCOOP_NET_ROLE')
local fired=false
local forceOnlyArmed=false
local function write(name,text,mode)
    local f=io.open(evidence..'/'..role..'-'..name,mode or 'w')
    if f then f:write(text);f:close() end
end
RegisterBeginPlayPreHook(function(param)
    if role~='host' or not forceOnlyArmed then return end
    local ok,err=pcall(function()
        local success,actor=pcall(function()return param:get()end)
        if not success then success,actor=pcall(function()return param:Get()end) end
        if not success then actor=param end
        if actor:IsValid() and actor:GetFullName():match('^explosion_C ') then
            actor.damage=0;actor.debris=0;actor.fireStrength=0;actor.fireFuel=0
            write('force-only.tsv',string.format('%s\tdamage=%.1f\tforce=%.1f\tradius=%.1f\n',
                os.date('!%H:%M:%S'),actor.damage,actor.force,actor.radius),'a')
        end
    end)
    if not ok then write('errors.txt',tostring(err)..'\n','a') end
end)
LoopAsync(1000,function()
    ExecuteInGameThread(function()
        local ok,err=pcall(function()
            local props=FindAllOf('prop_C') or {}
            local cans=FindAllOf('prop_gascan_C') or {}
            local lines={'props='..#props..' cans='..#cans}
            local byKey={}
            for _,p in ipairs(props) do
                if p:IsValid() then
                    local success,key=pcall(function() return p.Key:ToString() end)
                    if success and key~='None' then
                        byKey[key:lower()]=p
                        lines[#lines+1]=key..' '..p:GetFullName()
                    end
                end
            end
            write('props.txt',table.concat(lines,'\n'))
            local control=io.open(evidence..'/blast.txt','r')
            if not control then return end
            local command=control:read('*a');control:close()
            local sourceKey,targetKey,action=command:match('(%S+)%s+(%S+)%s+(%S+)')
            if not sourceKey then return end
            local source=byKey[sourceKey:lower()]
            local target=byKey[targetKey:lower()]
            if target and target:IsValid() then
                local at=target:K2_GetActorLocation()
                write('positions.tsv',string.format('%s\t%s\t%.3f\t%.3f\t%.3f\n',os.date('!%H:%M:%S'),targetKey,at.X,at.Y,at.Z),'a')
            end
            if role~='host' or fired or (action~='fire' and action~='force') or not source or not target then return end
            local player=nil
            for _,p in ipairs(FindAllOf('mainPlayer_C') or {}) do
                if p:IsValid() and p.Controller:IsValid() then player=p;break end
            end
            if not player then return end
            local victim=target:K2_GetActorLocation()
            local center={X=victim.X-200,Y=victim.Y,Z=victim.Z-50}
            source:K2_TeleportTo(center,{Pitch=0,Yaw=0,Roll=0})
            source.amount=100
            fired=true
            forceOnlyArmed=action=='force'
            source:broken()
            forceOnlyArmed=false
            write('stimulus.txt',sourceKey..' -> '..targetKey..' '..os.date('!%H:%M:%S'))
        end)
        if not ok then write('errors.txt',tostring(err)..'\n','a') end
    end,EGameThreadMethod.ProcessEvent)
    return false
end)
