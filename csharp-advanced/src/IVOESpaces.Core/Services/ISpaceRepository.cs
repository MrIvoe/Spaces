using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public interface ISpaceRepository
{
    Task<List<SpaceModel>> LoadAllAsync();
    Task SaveAllAsync(IEnumerable<SpaceModel> spaces);
    Task<SpaceModel> SaveOneAsync(SpaceModel space);
    Task DeleteAsync(Guid spaceId);
}
