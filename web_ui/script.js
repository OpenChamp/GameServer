const canvas = document.getElementById('mapCanvas');
const ctx = canvas.getContext('2d');

async function updateView() {
    try {
        const response = await fetch('/api/gamestate');
        const data = await response.json();

        // Clear canvas
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Draw grid first (background)
        drawPolygonGrid(data.map);

        // Draw map bounds
        drawMapBounds(data.map);

        // Draw navmesh
        drawNavmesh(data.map);

        // Draw spawnpoints
        drawSpawnpoints(data.map);

        // Draw entities
        drawEntities(data.entities);

        // Update state info
        updateStateInfo(data.state);

        // Update entities list
        updateEntitiesList(data.entities);
    } catch (e) {
        console.error('Failed to fetch game state:', e);
    }
}

function screenToWorld(screenX, screenY) {
    return {
        x: (screenX - 400) / 10,
        y: (screenY - 300) / 10
    };
}

function worldToScreen(worldX, worldY) {
    return {
        x: worldX * 10 + 400,
        y: worldY * 10 + 300
    };
}

function drawPolygonGrid(mapData) {
    if (!mapData || !mapData.grid) return;
    
    const grid = mapData.grid;
    if (grid.width === 0 || grid.height === 0 || !grid.cells || grid.cells.length === 0) return;
    
    const cellSize = grid.cell_size;
    const originX = grid.origin.x;
    const originY = grid.origin.y;
    
    // Grid visualization: very subtle outline to show spatial acceleration cells
    ctx.strokeStyle = '#3333ff';
    ctx.lineWidth = 0.5;
    ctx.fillStyle = 'rgba(51, 51, 255, 0.05)'; // Very transparent
    
    let drawnCount = 0;
    for (let cellData of grid.cells) {
        const x = cellData.x;
        const y = cellData.y;
        
        // Calculate cell bounds in world coordinates
        const minX = originX + (x * cellSize);
        const maxX = minX + cellSize;
        const minY = originY + (y * cellSize);
        const maxY = minY + cellSize;
        
        // Convert to screen coordinates
        const topLeft = worldToScreen(minX, minY);
        const topRight = worldToScreen(maxX, minY);
        const bottomLeft = worldToScreen(minX, maxY);
        const bottomRight = worldToScreen(maxX, maxY);
        
        // Skip if cell is completely outside canvas
        if ((topLeft.x > canvas.width && topRight.x > canvas.width && bottomRight.x > canvas.width) ||
            (topLeft.x < 0 && topRight.x < 0 && bottomLeft.x < 0) ||
            (topLeft.y > canvas.height && bottomLeft.y > canvas.height && bottomRight.y > canvas.height) ||
            (topLeft.y < 0 && topRight.y < 0 && topRight.y < 0)) {
            continue;
        }
        
        drawnCount++;
        
        // Draw cell rectangle (very subtle)
        ctx.beginPath();
        ctx.moveTo(topLeft.x, topLeft.y);
        ctx.lineTo(topRight.x, topRight.y);
        ctx.lineTo(bottomRight.x, bottomRight.y);
        ctx.lineTo(bottomLeft.x, bottomLeft.y);
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
        
        // Only draw labels if cell is reasonably sized on screen
        const cellWidth = Math.abs(topRight.x - topLeft.x);
        const cellHeight = Math.abs(bottomLeft.y - topLeft.y);
        
        if (cellWidth > 50 && cellHeight > 50) {
            const centerX = (topLeft.x + bottomRight.x) / 2;
            const centerY = (topLeft.y + bottomRight.y) / 2;
            ctx.fillStyle = '#3333ff';
            ctx.font = '8px monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(cellData.polygon_count + 'p', centerX, centerY);
        }
    }
    
    if (drawnCount > 0) {
        console.log(`Grid: Drew ${drawnCount} / ${grid.cells.length} cells`);
    }
}

function drawMapBounds(mapData) {
    if (!mapData) return;

    // Draw map boundary as a dashed line
    ctx.strokeStyle = '#664040';
    ctx.lineWidth = 2;
    ctx.setLineDash([10, 5]);

    const halfWidth = mapData.size.x / 2;
    const halfHeight = mapData.size.y / 2;

    const minX = mapData.offset.x - halfWidth;
    const maxX = mapData.offset.x + halfWidth;
    const minY = mapData.offset.y - halfHeight;
    const maxY = mapData.offset.y + halfHeight;

    const topLeft = worldToScreen(minX, minY);
    const topRight = worldToScreen(maxX, minY);
    const bottomLeft = worldToScreen(minX, maxY);
    const bottomRight = worldToScreen(maxX, maxY);

    ctx.beginPath();
    ctx.moveTo(topLeft.x, topLeft.y);
    ctx.lineTo(topRight.x, topRight.y);
    ctx.lineTo(bottomRight.x, bottomRight.y);
    ctx.lineTo(bottomLeft.x, bottomLeft.y);
    ctx.closePath();
    ctx.stroke();

    ctx.setLineDash([]);
}

function drawNavmesh(mapData) {
    if (!mapData || !mapData.vertices) return;

    const vertices = mapData.vertices;
    const polygons = mapData.polygons || [];

    // Draw polygon faces
    ctx.strokeStyle = '#408040';
    ctx.fillStyle = '#204020';
    ctx.lineWidth = 2;

    for (let poly of polygons) {
        if (poly.length < 3) continue;

        ctx.beginPath();
        const v = vertices[poly[0]];
        const screen = worldToScreen(v.x, v.z);
        ctx.moveTo(screen.x, screen.y);

        for (let i = 1; i < poly.length; i++) {
            const v = vertices[poly[i]];
            const screen = worldToScreen(v.x, v.z);
            ctx.lineTo(screen.x, screen.y);
        }
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
    }

    // Draw vertices with labels
    ctx.fillStyle = '#80ff80';
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 1;
    ctx.font = 'bold 9px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    for (let i = 0; i < vertices.length; i++) {
        const v = vertices[i];
        const screen = worldToScreen(v.x, v.z);

        // Draw vertex circle
        ctx.beginPath();
        ctx.arc(screen.x, screen.y, 3, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();

        // Draw vertex index
        ctx.fillStyle = '#000000';
        ctx.fillText(i.toString(), screen.x, screen.y);
        ctx.fillStyle = '#80ff80';
    }

    // Draw polygon centroids
    ctx.fillStyle = '#ff6b6b';
    for (let i = 0; i < polygons.length; i++) {
        const poly = polygons[i];
        if (poly.length < 3) continue;

        // Calculate centroid
        let cx = 0, cy = 0;
        for (let idx of poly) {
            cx += vertices[idx].x;
            cy += vertices[idx].z;
        }
        cx /= poly.length;
        cy /= poly.length;

        const screen = worldToScreen(cx, cy);
        ctx.beginPath();
        ctx.arc(screen.x, screen.y, 2, 0, Math.PI * 2);
        ctx.fill();
    }
}

function drawSpawnpoints(mapData) {
    if (!mapData || !mapData.spawnpoints) return;

    ctx.fillStyle = '#ffff00';
    ctx.strokeStyle = '#ffff00';
    ctx.lineWidth = 2;
    ctx.font = '12px Arial';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';

    for (let i = 0; i < mapData.spawnpoints.length; i++) {
        const sp = mapData.spawnpoints[i];
        const screen = worldToScreen(sp.x, sp.z);

        // Draw spawnpoint star
        ctx.beginPath();
        for (let j = 0; j < 5; j++) {
            const angle = (j * 4 * Math.PI) / 5 - Math.PI / 2;
            const radius = j % 2 === 0 ? 8 : 4;
            const x = screen.x + Math.cos(angle) * radius;
            const y = screen.y + Math.sin(angle) * radius;
            if (j === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.closePath();
        ctx.fill();
        ctx.stroke();

        // Draw label
        ctx.fillStyle = '#ffff00';
        ctx.fillText('S' + i, screen.x, screen.y - 12);
    }
}

function drawEntities(entities) {
    if (!entities) return;

    // First pass: draw targeting lines
    for (let entity of entities) {
        if (entity.target_id !== undefined) {
            const attacker_screen = worldToScreen(entity.position.x, entity.position.z);

            // Find target entity
            const target = entities.find(e => e.id === entity.target_id);
            if (target) {
                const target_screen = worldToScreen(target.position.x, target.position.z);

                // Draw targeting line
                ctx.strokeStyle = entity.team_id === 1 ? '#4080ff' : '#ff4080';
                ctx.lineWidth = 2;
                ctx.setLineDash([5, 5]);
                ctx.beginPath();
                ctx.moveTo(attacker_screen.x, attacker_screen.y);
                ctx.lineTo(target_screen.x, target_screen.y);
                ctx.stroke();
                ctx.setLineDash([]);

                // Draw arrowhead at target
                const angle = Math.atan2(target_screen.y - attacker_screen.y, target_screen.x - attacker_screen.x);
                const arrowSize = 8;

                ctx.fillStyle = entity.team_id === 1 ? '#4080ff' : '#ff4080';
                ctx.beginPath();
                ctx.moveTo(target_screen.x, target_screen.y);
                ctx.lineTo(target_screen.x - arrowSize * Math.cos(angle - Math.PI / 6), target_screen.y - arrowSize * Math.sin(angle - Math.PI / 6));
                ctx.lineTo(target_screen.x - arrowSize * Math.cos(angle + Math.PI / 6), target_screen.y - arrowSize * Math.sin(angle + Math.PI / 6));
                ctx.closePath();
                ctx.fill();
            }
        }
    }

    // Second pass: draw entities
    for (let entity of entities) {
        const screen = worldToScreen(entity.position.x, entity.position.z);

        // Color by team
        ctx.fillStyle = entity.team_id === 1 ? '#4040ff' : '#ff4040';
        ctx.beginPath();
        ctx.arc(screen.x, screen.y, 4, 0, Math.PI * 2);
        ctx.fill();

        // Draw ID
        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 10px Arial';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(entity.id.toString(), screen.x, screen.y);

        // Highlight if attacking
        if (entity.state === 'ATTACKING') {
            ctx.strokeStyle = '#ffff00';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.arc(screen.x, screen.y, 6, 0, Math.PI * 2);
            ctx.stroke();
        }

        // Draw state indicator
        let stateColor;
        switch (entity.state) {
            case 'ATTACKING': stateColor = '#ffff00'; break;
            case 'MOVING': stateColor = '#4da6ff'; break;
            case 'PATHFINDING_WAITING': stateColor = '#ffa500'; break;
            case 'DEAD': stateColor = '#ff0000'; break;
            default: stateColor = '#ffb347'; break;
        }
        ctx.strokeStyle = stateColor;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.arc(screen.x, screen.y, 8, 0, Math.PI * 2);
        ctx.stroke();
    }
}

function updateStateInfo(state) {
    document.getElementById('state').textContent = JSON.stringify(state, null, 2);
}

function updateEntitiesList(entities) {
    const div = document.getElementById('entities');
    div.innerHTML = '';

    if (!entities) return;

    for (let entity of entities) {
        const el = document.createElement('div');
        el.className = 'entity';

        let targetStr = '';
        if (entity.target_id !== undefined) {
            const target = entities.find(e => e.id === entity.target_id);
            const targetName = target ? `E${entity.target_id}` : `E${entity.target_id} (DEAD)`;
            targetStr = ` <span style="color: #ff8080">→ Targeting ${targetName}</span>`;
        }

        const stateColor = entity.state === 'ATTACKING' ? '#ffff00' :
            entity.state === 'MOVING' ? '#4da6ff' :
                entity.state === 'DEAD' ? '#ff0000' : '#ffb347';

        el.innerHTML = `<strong>E${entity.id}</strong> [Team ${entity.team_id}] | Intent: <span style="color: #4da6ff">${entity.intent}</span> | State: <span style="color: ${stateColor}"><b>${entity.state}</b></span>${targetStr} | (${entity.position.x.toFixed(1)}, ${entity.position.z.toFixed(1)})`;
        div.appendChild(el);
    }
}



// Update every 500ms
setInterval(updateView, 500);
updateView();