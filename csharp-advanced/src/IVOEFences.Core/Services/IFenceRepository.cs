using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public interface IFenceRepository
{
    Task<List<FenceModel>> LoadAllAsync();
    Task SaveAllAsync(IEnumerable<FenceModel> fences);
    Task<FenceModel> SaveOneAsync(FenceModel fence);
    Task DeleteAsync(Guid fenceId);
}
