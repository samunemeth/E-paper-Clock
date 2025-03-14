Import("env")

# TODO: Only on linux!
def before_upload(source, target, env):
    port = env.GetProjectOption("upload_port")
    print("Adding permission for upload port: " + port)
    env.Execute("sudo chmod a+rw " + port)


def after_upload(source, target, env):
    print("Upload finished successfully!")

env.AddPreAction("upload", before_upload)
env.AddPostAction("upload", after_upload)